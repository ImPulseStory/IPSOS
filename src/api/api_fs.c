#include "api.h"
#include <stdint.h>

int create_file(const char *name) {
    return ips_create_file(name);
}

int deleteEx(const char *name) {
    return ips_delete(name);
}

int read_file(const char *name, char* buf) {
    uint32_t out_size = 0;
    return ips_read_file(name, (uint8_t*)buf, 8192, &out_size);
}

int write_to_file(const char *name, const char* data) {
    uint32_t size = 0;
    while (data[size] != '\0') size++;
    return ips_write_file(name, data, size);
}

int list(char* buf, const char* path) {
    return ips_list(path, buf, 8192);
}

int mkdir(const char* path) {
    return ips_mkdir(path);
}

