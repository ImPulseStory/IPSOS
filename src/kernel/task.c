#include "../include/task_internal.h"
#include "../include/heap.h"
#include <stdint.h>

#define TASK_LIMIT 32u
#define TASK_STACK_SIZE 65536u
#define TASK_UNUSED 0u
#define TASK_RUNNABLE 1u
#define TASK_RUNNING 2u
#define TASK_SLEEPING 3u
#define TASK_WAIT_GROUP 4u
#define TASK_DEAD 5u

#define TASK_IDLE_INDEX 1u

extern void task_yield_stub(void);

static uint8_t idle_stack[4096] __attribute__((aligned(16)));

typedef struct {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_value;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} task_context_t;

typedef struct {
    uint32_t id;
    uint32_t group_id;
    uint32_t state;
    uint32_t wake_tick;
    uint32_t wait_group;
    uint32_t *saved_esp;
    uint8_t *stack;
    task_entry_t entry;
    void *arg;
    uint32_t permanent;
    uint8_t fpu_state[108] __attribute__((aligned(16)));
} task_t;

static task_t tasks[TASK_LIMIT];
static uint8_t initial_fpu_state[108] __attribute__((aligned(16)));
static task_t *current_task;
static uint32_t next_task_id = 1u;
static uint32_t next_group_id = 1u;
static uint32_t scheduler_ticks;
static uint32_t round_robin_cursor;
static uint32_t scheduler_ready;

static uint32_t irq_save(void)
{
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(uint32_t flags)
{
    if (flags & 0x200u)
        __asm__ volatile("sti" : : : "memory");
}

static void task_idle(void *arg)
{
    (void)arg;
    for (;;) {
        __asm__ volatile("sti; hlt" : : : "memory");
    }
}

static void task_bootstrap(void)
{
    task_entry_t entry = current_task->entry;
    void *arg = current_task->arg;
    entry(arg);
    task_exit();
}

static void task_prepare_context(task_t *task, uint8_t *stack, uint32_t stack_size)
{
    uintptr_t top = ((uintptr_t)stack + stack_size) & ~(uintptr_t)0x0Fu;
    uint32_t *return_slot = (uint32_t *)(top - sizeof(uint32_t));
    task_context_t *context = (task_context_t *)(top - sizeof(uint32_t) - sizeof(task_context_t));
    uint32_t *words = (uint32_t *)context;

    *return_slot = 0;
    for (uint32_t i = 0; i < sizeof(*context) / sizeof(uint32_t); ++i)
        words[i] = 0;

    context->gs = 0x10u;
    context->fs = 0x10u;
    context->es = 0x10u;
    context->ds = 0x10u;
    context->eip = (uint32_t)(uintptr_t)task_bootstrap;
    context->cs = 0x08u;
    context->eflags = 0x202u;
    task->saved_esp = (uint32_t *)context;
    task->stack = stack;
}


static int task_group_alive(uint32_t group_id)
{
    for (uint32_t i = 0; i < TASK_LIMIT; ++i) {
        if (tasks[i].state != TASK_UNUSED &&
            tasks[i].state != TASK_DEAD &&
            tasks[i].group_id == group_id)
            return 1;
    }
    return 0;
}

static void task_collect_group(uint32_t group_id)
{
    for (uint32_t i = 0; i < TASK_LIMIT; ++i) {
        task_t *task = &tasks[i];
        if (task->state == TASK_DEAD && task->group_id == group_id && task != current_task) {
            if (task->stack && !task->permanent)
                kfree(task->stack);
            for (uint32_t j = 0; j < sizeof(*task) / sizeof(uint32_t); ++j)
                ((uint32_t *)task)[j] = 0;
        }
    }
}

static void task_collect_detached(void)
{
    for (uint32_t i = 0; i < TASK_LIMIT; ++i) {
        task_t *task = &tasks[i];
        if (task->state == TASK_DEAD && task != current_task && task->group_id == 0u) {
            if (task->stack && !task->permanent)
                kfree(task->stack);
            for (uint32_t j = 0; j < sizeof(*task) / sizeof(uint32_t); ++j)
                ((uint32_t *)task)[j] = 0;
        }
    }
}

static uint32_t task_create_internal(task_entry_t entry, void *arg, uint32_t group_id, uint8_t *provided_stack, uint32_t provided_stack_size, uint32_t permanent)
{
    uint32_t flags;
    task_t *task = 0;
    uint8_t *stack = provided_stack;
    uint32_t stack_size = provided_stack_size;
    uint32_t id;

    if (!entry || !scheduler_ready)
        return 0;

    flags = irq_save();

    for (uint32_t i = 0; i < TASK_LIMIT; ++i) {
        if (tasks[i].state == TASK_UNUSED) {
            task = &tasks[i];
            break;
        }
    }

    if (!task) {
        irq_restore(flags);
        return 0;
    }

    if (!stack) {
        stack_size = TASK_STACK_SIZE;
        stack = (uint8_t *)kmalloc(stack_size);
        if (!stack) {
            irq_restore(flags);
            return 0;
        }
    }

    id = next_task_id++;
    if (!id)
        id = next_task_id++;

    task->id = id;
    task->group_id = group_id;
    task->state = TASK_RUNNABLE;
    task->wake_tick = 0;
    task->wait_group = 0;
    task->entry = entry;
    task->arg = arg;
    task->permanent = permanent;
    for (uint32_t i = 0; i < sizeof(task->fpu_state); ++i)
        task->fpu_state[i] = initial_fpu_state[i];
    task_prepare_context(task, stack, stack_size);

    irq_restore(flags);
    return id;
}

void task_init(void)
{
    uint32_t cr0;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~((1u << 2) | (1u << 3));
    cr0 |= (1u << 1);
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
    __asm__ volatile("fninit; fnsave %0" : "=m"(initial_fpu_state) : : "memory");

    for (uint32_t i = 0; i < TASK_LIMIT; ++i) {
        uint32_t *words = (uint32_t *)&tasks[i];
        for (uint32_t j = 0; j < sizeof(tasks[i]) / sizeof(uint32_t); ++j)
            words[j] = 0;
    }

    scheduler_ticks = 0;
    round_robin_cursor = 0;
    next_task_id = 2u;
    next_group_id = 1u;

    tasks[0].id = 1u;
    tasks[0].group_id = 0u;
    tasks[0].state = TASK_RUNNING;
    tasks[0].permanent = 1u;
    for (uint32_t i = 0; i < sizeof(tasks[0].fpu_state); ++i)
        tasks[0].fpu_state[i] = initial_fpu_state[i];

    tasks[TASK_IDLE_INDEX].id = 0u;
    tasks[TASK_IDLE_INDEX].group_id = 0u;
    tasks[TASK_IDLE_INDEX].state = TASK_RUNNABLE;
    tasks[TASK_IDLE_INDEX].entry = task_idle;
    tasks[TASK_IDLE_INDEX].permanent = 1u;
    for (uint32_t i = 0; i < sizeof(tasks[TASK_IDLE_INDEX].fpu_state); ++i)
        tasks[TASK_IDLE_INDEX].fpu_state[i] = initial_fpu_state[i];
    task_prepare_context(&tasks[TASK_IDLE_INDEX], idle_stack, sizeof(idle_stack));

    current_task = &tasks[0];
    scheduler_ready = 1u;
}

uint32_t task_create(task_entry_t entry, void *arg)
{
    uint32_t group_id = current_task ? current_task->group_id : 0u;
    return task_create_internal(entry, arg, group_id, 0, 0, 0);
}

uint32_t task_create_group(task_entry_t entry, void *arg, uint32_t *group_id)
{
    uint32_t group;
    uint32_t flags;
    uint32_t id;

    if (!scheduler_ready)
        return 0;

    flags = irq_save();
    group = next_group_id++;
    if (!group)
        group = next_group_id++;
    irq_restore(flags);

    id = task_create_internal(entry, arg, group, 0, 0, 0);
    if (!id)
        return 0;

    if (group_id)
        *group_id = group;
    return id;
}

void task_wait_group(uint32_t group_id)
{
    if (!group_id || !scheduler_ready)
        return;

    for (;;) {
        uint32_t flags = irq_save();

        if (!task_group_alive(group_id)) {
            task_collect_group(group_id);
            irq_restore(flags);
            return;
        }

        if (current_task && current_task->state == TASK_RUNNING) {
            current_task->state = TASK_WAIT_GROUP;
            current_task->wait_group = group_id;
        }

        irq_restore(flags);
        task_yield();
    }
}

void task_yield(void)
{
    if (!scheduler_ready)
        return;
    __asm__ volatile("int $0x30" : : : "memory");
}

void task_sleep(uint32_t milliseconds)
{
    uint32_t flags;
    uint32_t ticks;

    if (!scheduler_ready || !current_task)
        return;

    if (!milliseconds) {
        task_yield();
        return;
    }

    ticks = milliseconds / 10u + ((milliseconds % 10u) != 0u);
    if (!ticks)
        ticks = 1u;

    flags = irq_save();
    current_task->wake_tick = scheduler_ticks + ticks;
    current_task->state = TASK_SLEEPING;
    irq_restore(flags);

    task_yield();
}

void task_exit(void)
{
    uint32_t flags;

    if (!scheduler_ready || !current_task)
        for (;;) __asm__ volatile("cli; hlt" : : : "memory");

    flags = irq_save();
    current_task->state = TASK_DEAD;
    irq_restore(flags);
    task_yield();

    for (;;) __asm__ volatile("cli; hlt" : : : "memory");
}

void task_exit_group(void)
{
    uint32_t group_id;
    uint32_t flags;

    if (!scheduler_ready || !current_task || !current_task->group_id)
        task_exit();

    flags = irq_save();
    group_id = current_task->group_id;
    for (uint32_t i = 0; i < TASK_LIMIT; ++i) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].group_id == group_id)
            tasks[i].state = TASK_DEAD;
    }
    irq_restore(flags);

    task_yield();
    for (;;) __asm__ volatile("cli; hlt" : : : "memory");
}

uint32_t task_get_id(void)
{
    return current_task ? current_task->id : 0u;
}

void ips_process_exit(int status)
{
    (void)status;
    task_exit_group();
}

void ips_process_abort(void)
{
    task_exit_group();
}

uint32_t *task_schedule(uint32_t *saved_esp, uint32_t from_timer)
{
    task_t *next = 0;

    if (!scheduler_ready || !current_task)
        return saved_esp;

    if (from_timer)
        ++scheduler_ticks;

    current_task->saved_esp = saved_esp;
    __asm__ volatile("fnsave %0" : "=m"(current_task->fpu_state) : : "memory");

    if (current_task->state == TASK_RUNNING)
        current_task->state = TASK_RUNNABLE;

    for (uint32_t i = 0; i < TASK_LIMIT; ++i) {
        task_t *task = &tasks[i];
        if (task->state == TASK_SLEEPING && (int32_t)(scheduler_ticks - task->wake_tick) >= 0)
            task->state = TASK_RUNNABLE;
        if (task->state == TASK_WAIT_GROUP && !task_group_alive(task->wait_group))
            task->state = TASK_RUNNABLE;
    }

    task_collect_detached();

    for (uint32_t step = 1; step <= TASK_LIMIT; ++step) {
        uint32_t index = (round_robin_cursor + step) % TASK_LIMIT;
        task_t *candidate = &tasks[index];
        if (candidate != current_task && index != TASK_IDLE_INDEX && candidate->state == TASK_RUNNABLE) {
            next = candidate;
            round_robin_cursor = index;
            break;
        }
    }

    if (!next && current_task->state == TASK_RUNNABLE)
        next = current_task;

    if (!next)
        next = &tasks[TASK_IDLE_INDEX];

    next->state = TASK_RUNNING;
    current_task = next;
    __asm__ volatile("frstor %0" : : "m"(next->fpu_state) : "memory");
    return next->saved_esp;
}
