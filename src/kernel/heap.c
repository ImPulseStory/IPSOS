#include "../include/heap.h"
#include <stdint.h>
#include <stddef.h>

extern uint8_t kernel_end;

typedef struct heap_block {
    size_t size;
    struct heap_block *next;
    uint32_t magic;
    uint32_t free;
} heap_block_t;

#define HEAP_MAGIC 0x49505348u
#define HEAP_ALIGNMENT 8u
#define HEAP_LIMIT 0x00B00000u
#define HEAP_RESERVED_START 0x000A0000u
#define HEAP_RESERVED_END   0x00100000u

static heap_block_t *heap_first;
static heap_block_t *heap_last;
static uintptr_t heap_next = (uintptr_t)&kernel_end;

static uint32_t heap_irq_save(void)
{
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void heap_irq_restore(uint32_t flags)
{
    if (flags & 0x200u)
        __asm__ volatile("sti" : : : "memory");
}

static uintptr_t heap_align(uintptr_t value)
{
    return (value + (HEAP_ALIGNMENT - 1u)) & ~(uintptr_t)(HEAP_ALIGNMENT - 1u);
}

static uintptr_t heap_skip_reserved(uintptr_t address, size_t span)
{
    if (address >= HEAP_RESERVED_START && address < HEAP_RESERVED_END)
        return heap_align(HEAP_RESERVED_END);

    if (address < HEAP_RESERVED_START &&
        span > (size_t)(HEAP_RESERVED_START - address))
        return heap_align(HEAP_RESERVED_END);

    return address;
}

static void heap_split(heap_block_t *block, size_t size)
{
    uintptr_t next_address;
    heap_block_t *next;

    if (block->size < size + sizeof(heap_block_t) + HEAP_ALIGNMENT)
        return;

    next_address = heap_align((uintptr_t)(block + 1) + size);
    next = (heap_block_t *)next_address;
    next->size = block->size - (size_t)(next_address - (uintptr_t)(block + 1)) - sizeof(heap_block_t);
    next->next = block->next;
    next->magic = HEAP_MAGIC;
    next->free = 1;

    block->size = (size_t)(next_address - (uintptr_t)(block + 1));
    block->next = next;

    if (heap_last == block)
        heap_last = next;
}

static void heap_coalesce(void)
{
    heap_block_t *block = heap_first;

    while (block && block->next) {
        heap_block_t *next = block->next;
        uintptr_t block_end = heap_align((uintptr_t)(block + 1) + block->size);

        if (block->free && next->free && block_end == (uintptr_t)next) {
            block->size = (size_t)((uintptr_t)(next + 1) + next->size - (uintptr_t)(block + 1));
            block->next = next->next;
            if (heap_last == next)
                heap_last = block;
        } else {
            block = next;
        }
    }
}

static void *kmalloc_unlocked(uint32_t requested_size)
{
    size_t size = requested_size ? (size_t)requested_size : 1u;
    heap_block_t *block;
    uintptr_t block_address;

    if (size > (size_t)(UINTPTR_MAX - sizeof(heap_block_t) - HEAP_ALIGNMENT))
        return NULL;

    size = (size + HEAP_ALIGNMENT - 1u) & ~(size_t)(HEAP_ALIGNMENT - 1u);
    if (size > HEAP_LIMIT - sizeof(heap_block_t))
        return NULL;

    for (block = heap_first; block; block = block->next) {
        if (block->free && block->size >= size) {
            heap_split(block, size);
            block->free = 0;
            return (void *)(block + 1);
        }
    }

    block_address = heap_align(heap_next);
    block_address = heap_skip_reserved(block_address, sizeof(heap_block_t) + size);
    if (block_address > UINTPTR_MAX - sizeof(heap_block_t) - size ||
        block_address > HEAP_LIMIT - sizeof(heap_block_t) - size)
        return NULL;

    block = (heap_block_t *)block_address;
    block->size = size;
    block->next = NULL;
    block->magic = HEAP_MAGIC;
    block->free = 0;

    if (heap_last)
        heap_last->next = block;
    else
        heap_first = block;

    heap_last = block;
    heap_next = heap_align((uintptr_t)(block + 1) + size);

    return (void *)(block + 1);
}

static void kfree_unlocked(void *ptr)
{
    heap_block_t *block;

    if (!ptr)
        return;

    block = ((heap_block_t *)ptr) - 1;
    if (block->magic != HEAP_MAGIC || block->free)
        return;

    block->free = 1;
    heap_coalesce();
}

static void *krealloc_unlocked(void *ptr, uint32_t requested_size)
{
    heap_block_t *block;
    void *new_ptr;
    size_t copy_size;

    if (!ptr)
        return kmalloc_unlocked(requested_size);

    if (!requested_size) {
        kfree_unlocked(ptr);
        return NULL;
    }

    block = ((heap_block_t *)ptr) - 1;
    if (block->magic != HEAP_MAGIC || block->free)
        return NULL;

    if (block->size >= requested_size) {
        heap_split(block, requested_size);
        return ptr;
    }

    new_ptr = kmalloc_unlocked(requested_size);
    if (!new_ptr)
        return NULL;

    copy_size = block->size;
    {
        uint8_t *dst = (uint8_t *)new_ptr;
        const uint8_t *src = (const uint8_t *)ptr;
        for (size_t i = 0; i < copy_size; ++i)
            dst[i] = src[i];
    }

    kfree_unlocked(ptr);
    return new_ptr;
}


void *kmalloc(uint32_t requested_size)
{
    uint32_t flags = heap_irq_save();
    void *result = kmalloc_unlocked(requested_size);
    heap_irq_restore(flags);
    return result;
}

void kfree(void *ptr)
{
    uint32_t flags = heap_irq_save();
    kfree_unlocked(ptr);
    heap_irq_restore(flags);
}

void *krealloc(void *ptr, uint32_t requested_size)
{
    uint32_t flags = heap_irq_save();
    void *result = krealloc_unlocked(ptr, requested_size);
    heap_irq_restore(flags);
    return result;
}

void *malloc(size_t size)
{
    if (size > UINT32_MAX)
        return NULL;
    return kmalloc((uint32_t)size);
}

void free(void *ptr)
{
    kfree(ptr);
}

void *calloc(size_t count, size_t size)
{
    size_t total;
    uint8_t *ptr;

    if (size && count > SIZE_MAX / size)
        return NULL;

    total = count * size;
    if (total > UINT32_MAX)
        return NULL;

    ptr = (uint8_t *)kmalloc((uint32_t)total);
    if (!ptr)
        return NULL;

    for (size_t i = 0; i < total; ++i)
        ptr[i] = 0;

    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    if (size > UINT32_MAX)
        return NULL;
    return krealloc(ptr, (uint32_t)size);
}
