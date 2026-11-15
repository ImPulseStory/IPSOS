#include "../include/libc.h"
#include <stddef.h>

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
    while (src[j] != '\0') { dest[i] = src[j]; i++; j++; }
    dest[i] = '\0';
}

size_t my_strlen(const char* str) {
    size_t len = 0;
    if (!str) return 0; 
    
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

char* my_strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest; 
    
    char* ptr = dest;
    while (*src != '\0') {
        *ptr = *src;
        ptr++;
        src++;
    }
    *ptr = '\0'; 
    return dest;
}