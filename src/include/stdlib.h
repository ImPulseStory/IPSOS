#ifndef IPSOS_STDLIB_H
#define IPSOS_STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);
long strtol(const char *str, char **endptr, int base);
float strtof(const char *str, char **endptr);
long double strtold(const char *str, char **endptr);
char *realpath(const char *path, char *resolved_path);
unsigned long strtoul(const char *str, char **endptr, int base);
long long strtoll(const char *str, char **endptr, int base);
unsigned long long strtoull(const char *str, char **endptr, int base);
int atoi(const char *str);
long atol(const char *str);
int abs(int value);
long labs(long value);
void qsort(void *base, size_t count, size_t size, int (*compare)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t count, size_t size, int (*compare)(const void *, const void *));
char *getenv(const char *name);
int atexit(void (*function)(void));
void abort(void);
void exit(int status);
void _Exit(int status);
int rand(void);
void srand(unsigned int seed);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

#endif
