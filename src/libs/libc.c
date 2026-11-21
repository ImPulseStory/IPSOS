#include "../include/libc.h"

void* memcpy(void* dest, const void* src, size_t n) {
    if (!dest || !src) return dest;
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memset(void* s, int c, size_t n) {
    if (!s) return s;
    unsigned char* p = (unsigned char*)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void* memmove(void* dest, const void* src, size_t n) {
    if (!dest || !src) return dest;
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    if (!s1 || !s2) return 0;
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

size_t strlen(const char* s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

char* strcpy(char*  dest, const char*  src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}

char* strncpy(char*  dest, const char*  src, size_t n) {
    if (!dest || !src) return dest;
    char* d = dest;
    while (n && *src) {
        *d++ = *src++;
        n--;
    }
    while (n--) {
        *d++ = '\0';
    }
    return dest;
}

char* strcat(char*  dest, const char*  src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++) != '\0');
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0;
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2) return 0;
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strchr(const char* s, int c) {
    if (!s) return NULL;
    while (*s != (char)c) {
        if (!*s++) return NULL;
    }
    return (char*)s;
}

char* strrchr(const char* s, int c) {
    if (!s) return NULL;
    const char* last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char*)last;
}

static char heap[1024 * 1024]; 
static size_t heap_index = 0;

void* malloc(size_t size) {
    if (heap_index + size > sizeof(heap)) return NULL;
    void* ptr = &heap[heap_index];
    heap_index += size;
    if (heap_index % 4 != 0) heap_index += 4 - (heap_index % 4);
    return ptr;
}

void free(void* ptr) {
    (void)ptr; 
}

void* calloc(size_t nmemb, size_t size) {
    void* ptr = malloc(nmemb * size);
    if (ptr) memset(ptr, 0, nmemb * size);
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    void* new_ptr = malloc(size);
    if (ptr && new_ptr) {
        memcpy(new_ptr, ptr, size); 
    }
    return new_ptr;
}

int open(const char *pathname, int flags, ...) {
    return -1; 
}

int close(int fd) {
    return 0;
}

ssize_t read(int fd, void *buf, size_t count) {
    return 0;
}

ssize_t write(int fd, const void *buf, size_t count) {
    return count;
}

void exit(int status) {
    while(1); 
}

long long __divdi3(long long a, long long b) { return a / b; }
unsigned long long __udivdi3(unsigned long long a, unsigned long long b) { return a / b; }
long long __moddi3(long long a, long long b) { return a % b; }
unsigned long long __umoddi3(unsigned long long a, unsigned long long b) { return a % b; }