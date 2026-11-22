#include "api.h"
#include <stdint.h>

int create_file(const char *name) {
    return ips_create_file(name);
}

int deleteEx(const char *name) {
    return ips_delete(name);
}

int read_file(
    const char *name,
    char *buf,
    uint32_t capacity,
    uint32_t *out_size
) {
    return ips_read_file(
        name,
        (uint8_t*)buf,
        capacity,
        out_size
    );
}

int write_to_file(const char *name, const char* data) {
    uint32_t size = 0;
    while (data[size] != '\0') size++;
    return ips_write_file(name, data, size);
}

int list(char *buf, const char *path) {
    return ips_list(path, buf, 8192);
}

int list_ex(char *buf, const char *path, uint32_t capacity) {
    if (!buf || !path || capacity == 0 || capacity > 0x7FFFFFFFu)
        return -1;
    return ips_list(path, buf, (int)capacity);
}

int mkdir(const char* path) {
    return ips_mkdir(path);
}

