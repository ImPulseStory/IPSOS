#include "../include/ipsfs.h"
#include <stdint.h>

/* Generated from examples/cube3d.c by objcopy at build time. */
extern const uint8_t _binary_examples_cube3d_c_start[];
extern const uint8_t _binary_examples_cube3d_c_end[];

void ips_seed_examples(void)
{
    const char *dir = "/apps";
    const char *path = "/apps/cube3d.c";
    uint32_t size;

    /* Existing user files are deliberately never overwritten. */
    if (ips_resolve_path(path) >= 0)
        return;

    if (ips_resolve_path(dir) < 0)
        (void)ips_mkdir(dir);

    if (ips_resolve_path(path) >= 0)
        return;

    if (ips_create_file(path) != 0)
        return;

    size = (uint32_t)(_binary_examples_cube3d_c_end - _binary_examples_cube3d_c_start);
    if (ips_write_file(path, _binary_examples_cube3d_c_start, size) != (int)size)
        (void)ips_delete(path);
}
