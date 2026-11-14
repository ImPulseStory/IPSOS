#include "../include/libc.h"

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

void strcat(char* dest, const char* src) {
    int i = 0;
    while (dest[i] != '\0') i++;
    int j = 0;
    while (src[j] != '\0') dest[i] = src[j]; i++; j++;
    dest[i] = '\0';
}