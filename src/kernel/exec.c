#include "../include/exec.h"
#include "../include/window.h"

extern void window_destroy_owner(unsigned int owner_task);
#include "../include/ipsfs.h"
#include "../include/heap.h"
#include "../include/stdlib.h"
#include "../include/unistd.h"
#include "../include/task_internal.h"
#include "../apps/terminal/terminal.h"
#include <stdint.h>
#include <stddef.h>

#define EXEC_FILE_LIMIT (BLOCK_SIZE * DIRECT_BLOCKS)
#define EXEC_LOAD_START 0x00C00000u
#define EXEC_LOAD_END   0x00F00000u
#define EXEC_MAX_ARGS   16u
#define EXEC_ARG_SIZE   512u

#define ELF_PT_LOAD 1u
#define ELF_PF_X 1u
#define ELF_ET_EXEC 2u
#define ELF_EM_386 3u

typedef struct __attribute__((packed)) {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} exec_elf_header_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} exec_elf_program_header_t;

typedef int (*exec_main_t)(int argc, char **argv, char **envp);

typedef struct {
    exec_main_t entry;
    int argc;
    char **argv;
    char *argument_storage;
    volatile int result;
} exec_context_t;

static void exec_program_task(void *arg)
{
    exec_context_t *context = (exec_context_t *)arg;
    context->result = context->entry(context->argc, context->argv, 0);
    window_destroy_owner(task_get_id());
    task_exit();
}

static int exec_build_arguments(exec_context_t *context, const char *path, const char *arguments)
{
    uint32_t used = 0;
    uint32_t length = 0;

    context->argv = (char **)kmalloc(sizeof(char *) * (EXEC_MAX_ARGS + 1u));
    context->argument_storage = (char *)kmalloc(EXEC_ARG_SIZE);
    if (!context->argv || !context->argument_storage)
        return -1;

    while (path[length]) {
        if (used + length + 1u >= EXEC_ARG_SIZE)
            return -1;
        context->argument_storage[used + length] = path[length];
        ++length;
    }
    context->argument_storage[used + length] = '\0';
    context->argv[0] = context->argument_storage;
    used += length + 1u;
    context->argc = 1;

    while (arguments && *arguments) {
        while (*arguments == ' ' || *arguments == '\t')
            ++arguments;
        if (!*arguments)
            break;
        if ((uint32_t)context->argc >= EXEC_MAX_ARGS)
            return -1;
        context->argv[context->argc++] = context->argument_storage + used;
        while (*arguments && *arguments != ' ' && *arguments != '\t') {
            if (used + 1u >= EXEC_ARG_SIZE)
                return -1;
            context->argument_storage[used++] = *arguments++;
        }
        context->argument_storage[used++] = '\0';
    }

    context->argv[context->argc] = 0;
    return 0;
}

static int exec_validate_and_load(const uint8_t *file, uint32_t file_size, uint32_t *entry_out)
{
    const exec_elf_header_t *header;
    const exec_elf_program_header_t *program_headers;
    uint32_t entry_found = 0;

    if (file_size < sizeof(exec_elf_header_t))
        return -1;

    header = (const exec_elf_header_t *)file;
    if (header->ident[0] != 0x7Fu || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != 1u || header->ident[5] != 1u ||
        header->ident[6] != 1u || header->type != ELF_ET_EXEC ||
        header->machine != ELF_EM_386 || header->version != 1u ||
        header->ehsize != sizeof(exec_elf_header_t) ||
        header->phentsize != sizeof(exec_elf_program_header_t) ||
        !header->phnum)
        return -1;

    if (header->phoff > file_size ||
        header->phnum > (file_size - header->phoff) / sizeof(exec_elf_program_header_t))
        return -1;

    program_headers = (const exec_elf_program_header_t *)(file + header->phoff);

    for (uint32_t i = 0; i < header->phnum; ++i) {
        const exec_elf_program_header_t *program = &program_headers[i];
        uint32_t segment_end;

        if (program->type != ELF_PT_LOAD)
            continue;

        if (program->filesz > program->memsz ||
            program->offset > file_size ||
            program->filesz > file_size - program->offset ||
            program->vaddr < EXEC_LOAD_START ||
            program->vaddr >= EXEC_LOAD_END ||
            program->memsz > EXEC_LOAD_END - program->vaddr)
            return -1;

        segment_end = program->vaddr + program->memsz;
        if (segment_end < program->vaddr)
            return -1;

        if ((program->flags & ELF_PF_X) &&
            header->entry >= program->vaddr && header->entry < segment_end)
            entry_found = 1u;
    }

    if (!entry_found)
        return -1;

    for (uint32_t i = 0; i < header->phnum; ++i) {
        const exec_elf_program_header_t *program = &program_headers[i];
        uint8_t *destination;
        if (program->type != ELF_PT_LOAD)
            continue;
        destination = (uint8_t *)(uintptr_t)program->vaddr;
        for (uint32_t j = 0; j < program->memsz; ++j)
            destination[j] = 0;
    }

    for (uint32_t i = 0; i < header->phnum; ++i) {
        const exec_elf_program_header_t *program = &program_headers[i];
        uint8_t *destination;
        if (program->type != ELF_PT_LOAD)
            continue;
        destination = (uint8_t *)(uintptr_t)program->vaddr;
        for (uint32_t j = 0; j < program->filesz; ++j)
            destination[j] = file[program->offset + j];
    }

    *entry_out = header->entry;
    return 0;
}

int exec_elf(const char *path, const char *arguments)
{
    char normalized_path[512];
    char previous_directory[256];
    uint8_t *file;
    uint32_t file_size = 0;
    uint32_t entry_address = 0;
    uint32_t group_id = 0;
    exec_context_t *context;
    int result;

    if (!path || !*path || !realpath(path, normalized_path))
        return -1;

    previous_directory[0] = '\0';
    getcwd(previous_directory, sizeof(previous_directory));

    file = (uint8_t *)kmalloc(EXEC_FILE_LIMIT);
    if (!file) {
        term_print("EXEC: not enough memory\n");
        return -1;
    }

    if (ips_read_file(normalized_path, file, EXEC_FILE_LIMIT, &file_size) != 0) {
        term_print("EXEC: cannot read file\n");
        kfree(file);
        return -1;
    }

    if (exec_validate_and_load(file, file_size, &entry_address) != 0) {
        term_print("EXEC: invalid ELF32 executable\n");
        kfree(file);
        return -1;
    }

    kfree(file);

    context = (exec_context_t *)kmalloc(sizeof(exec_context_t));
    if (!context) {
        term_print("EXEC: not enough memory\n");
        return -1;
    }

    context->entry = (exec_main_t)(uintptr_t)entry_address;
    context->argc = 0;
    context->argv = 0;
    context->argument_storage = 0;
    context->result = 0;

    if (exec_build_arguments(context, normalized_path, arguments) != 0) {
        term_print("EXEC: invalid argument list\n");
        kfree(context->argv);
        kfree(context->argument_storage);
        kfree(context);
        return -1;
    }

    if (!task_create_group(exec_program_task, context, &group_id)) {
        term_print("EXEC: cannot create task\n");
        kfree(context->argv);
        kfree(context->argument_storage);
        kfree(context);
        return -1;
    }

    task_wait_group(group_id);
    result = context->result;
    if (previous_directory[0])
        chdir(previous_directory);

    kfree(context->argv);
    kfree(context->argument_storage);
    kfree(context);

    return result;
}
