#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>

int strcmp(const char* a, const char* b);
char* my_strcpy(char* dest, const char* src);
size_t my_strlen(const char* str);
void strcat(char* dest, const char* src);

#endif