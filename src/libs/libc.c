#include "../include/libc.h"
#include "../include/stdlib.h"
#include "../include/ctype.h"
#include "../include/errno.h"
#include "../include/limits.h"
#include <stdint.h>
#include <stddef.h>

int errno;

static unsigned int random_seed = 1u;
static char *strtok_state;

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (!dest || !src)
        return dest;

    while (n--)
        *d++ = *s++;

    return dest;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;

    if (!s)
        return s;

    while (n--)
        *p++ = (unsigned char)c;

    return s;
}

void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (!dest || !src)
        return dest;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) 
            *--d = *--s;
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;

    if (!s1 || !s2)
        return 0;

    while (n--) {
        if (*a != *b)
            return (int)*a - (int)*b;
        ++a;
        ++b;
    }

    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;

    if (!s)
        return NULL;

    while (n--) {
        if (*p == (unsigned char)c)
            return (void *)p;
        ++p;
    }

    return NULL;
}

size_t strlen(const char *s)
{
    size_t len = 0;

    if (!s)
        return 0;

    while (s[len])
        ++len;

    return len;
}

size_t strnlen(const char *s, size_t n)
{
    size_t len = 0;

    if (!s)
        return 0;

    while (len < n && s[len])
        ++len;

    return len;
}

char *strcpy(char *dest, const char *src)
{
    char *out = dest;

    if (!dest || !src)
        return dest;

    while ((*dest++ = *src++) != '\0')
    ;

    return out;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *out = dest;

    if (!dest || !src)
        return dest;

    while (n && *src) {
        *dest++ = *src++;
        --n;
    }

    while (n--)
        *dest++ = '\0';

    return out;
}

char *strcat(char *dest, const char *src)
{
    char *out = dest;

    if (!dest || !src)
        return dest;

    while (*dest)
        ++dest;

    while ((*dest++ = *src++) != '\0')
    ;

    return out;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *out = dest;

    if (!dest || !src)
        return dest;

    while (*dest)
        ++dest;

    while (n && *src) {
        *dest++ = *src++;
        --n;
    }

    *dest = '\0';
    return out;
}

int strcasecmp(const char *s1, const char *s2)
{
    if (!s1 || !s2)
        return 0;

    while (*s1 && tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
        ++s1;
        ++s2;
    }

    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
    if (!s1 || !s2)
        return 0;

    while (n && *s1 && tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
        ++s1;
        ++s2;
        --n;
    }

    if (!n)
        return 0;

    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strcmp(const char *s1, const char *s2)
{
    if (!s1 || !s2)
        return 0;

    while (*s1 && *s1 == *s2) {
        ++s1;
        ++s2;
    }

    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    if (!s1 || !s2)
        return 0;

    while (n && *s1 && *s1 == *s2) {
        ++s1;
        ++s2;
        --n;
    }

    if (!n)
        return 0;

    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

char *strchr(const char *s, int c)
{
    if (!s)
        return NULL;

    do {
        if (*s == (char)c)
            return (char *)s;
    } while (*s++);

    return NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;

    if (!s)
        return NULL;

    do {
        if (*s == (char)c)
            last = s;
    } while (*s++);

    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    size_t needle_len;

    if (!haystack || !needle)
        return NULL;

    if (!*needle)
        return (char *)haystack;

    needle_len = strlen(needle);
    while (*haystack) {
        if (*haystack == *needle && strncmp(haystack, needle, needle_len) == 0)
            return (char *)haystack;
        ++haystack;
    }

    return NULL;
}

char *strdup(const char *s)
{
    size_t len;
    char *copy;

    if (!s)
        return NULL;

    len = strlen(s);
    copy = (char *)malloc(len + 1u);
    if (!copy)
        return NULL;

    memcpy(copy, s, len + 1u);
    return copy;
}

char *strndup(const char *s, size_t n)
{
    size_t len;
    char *copy;

    if (!s)
        return NULL;

    len = strnlen(s, n);
    copy = (char *)malloc(len + 1u);
    if (!copy)
        return NULL;

    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

static int string_has_char(const char *s, char c)
{
    while (*s) {
        if (*s++ == c)
            return 1;
    }
    return 0;
}

size_t strspn(const char *s, const char *accept)
{
    size_t n = 0;

    if (!s || !accept)
        return 0;

    while (s[n] && string_has_char(accept, s[n]))
        ++n;

    return n;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t n = 0;

    if (!s || !reject)
        return 0;

    while (s[n] && !string_has_char(reject, s[n]))
        ++n;

    return n;
}

char *strpbrk(const char *s, const char *accept)
{
    if (!s || !accept)
        return NULL;

    while (*s) {
        if (string_has_char(accept, *s))
            return (char *)s;
        ++s;
    }

    return NULL;
}

char *strtok(char *str, const char *delim)
{
    char *start;

    if (!delim)
        return NULL;

    if (str)
        strtok_state = str;

    if (!strtok_state)
        return NULL;

    while (*strtok_state && string_has_char(delim, *strtok_state))
        ++strtok_state;

    if (!*strtok_state) {
        strtok_state = NULL;
        return NULL;
    }

    start = strtok_state;
    while (*strtok_state && !string_has_char(delim, *strtok_state))
        ++strtok_state;

    if (*strtok_state)
        *strtok_state++ = '\0';
    else
        strtok_state = NULL;

    return start;
}

int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

int islower(int c)
{
    return c >= 'a' && c <= 'z';
}

int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

int isalpha(int c)
{
    return islower(c) || isupper(c);
}

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int isascii(int c)
{
    return (unsigned int)c <= 0x7Fu;
}

int isblank(int c)
{
    return c == ' ' || c == '\t';
}

int iscntrl(int c)
{
    return (c >= 0 && c < 0x20) || c == 0x7F;
}

int isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int isprint(int c)
{
    return c >= 0x20 && c <= 0x7E;
}

int isgraph(int c)
{
    return c >= 0x21 && c <= 0x7E;
}

int ispunct(int c)
{
    return isgraph(c) && !isalnum(c);
}

int isxdigit(int c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int tolower(int c)
{
    return isupper(c) ? c + ('a' - 'A') : c;
}

int toupper(int c)
{
    return islower(c) ? c - ('a' - 'A') : c;
}

static int digit_value(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return -1;
}

static uint32_t ull_div_small(unsigned long long value, uint32_t divisor, unsigned long long *quotient)
{
    uint32_t high = (uint32_t)(value >> 32);
    uint32_t low = (uint32_t)value;
    uint32_t quotient_high = high / divisor;
    uint32_t remainder = high % divisor;

    __asm__ volatile ("divl %2" : "+a"(low), "+d"(remainder) : "rm"(divisor));
    *quotient = ((unsigned long long)quotient_high << 32) | low;
    return remainder;
}

static int ull_mul_add_small(unsigned long long value, uint32_t multiplier, uint32_t addend, unsigned long long *result)
{
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    unsigned long long low_product = (unsigned long long)low * multiplier + addend;
    unsigned long long high_product = (unsigned long long)high * multiplier + (uint32_t)(low_product >> 32);

    if (high_product > UINT32_MAX)
        return -1;

    *result = (high_product << 32) | (uint32_t)low_product;
    return 0;
}

unsigned long long strtoull(const char *str, char **endptr, int base)
{
    const char *p = str;
    unsigned long long value = 0;
    unsigned long long limit = ~(unsigned long long)0;
    int negative = 0;
    int any = 0;

    errno = 0;

    if (!str) {
        if (endptr)
            *endptr = NULL;
        errno = EINVAL;
        return 0;
    }

    while (isspace((unsigned char)*p))
        ++p;

    if (*p == '+' || *p == '-') {
        negative = (*p == '-');
        ++p;
    }

    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        if (base == 0)
            base = 16;
    } else if (base == 0) {
        base = (*p == '0') ? 8 : 10;
    }

    if (base < 2 || base > 36) {
        if (endptr)
            *endptr = (char *)str;
        errno = EINVAL;
        return 0;
    }

    while (*p) {
        int digit = digit_value((unsigned char)*p);
        if (digit < 0 || digit >= base)
            break;
        unsigned long long maximum_quotient;
        uint32_t maximum_remainder = ull_div_small(limit, (uint32_t)base, &maximum_quotient);
        unsigned long long next_value;
        any = 1;
        if (value > maximum_quotient || (value == maximum_quotient && (uint32_t)digit > maximum_remainder) ||
            ull_mul_add_small(value, (uint32_t)base, (uint32_t)digit, &next_value) != 0) {
            value = limit;
            errno = ERANGE;
            while (p[1]) {
                int next_digit = digit_value((unsigned char)p[1]);
                if (next_digit < 0 || next_digit >= base)
                    break;
                ++p;
            }
            ++p;
            break;
        }
        value = next_value;
        ++p;
    }

    if (endptr)
        *endptr = (char *)(any ? p : str);

    if (errno == ERANGE)
        return limit;

    if (negative)
        return 0ull - value;

    return value;
}

unsigned long strtoul(const char *str, char **endptr, int base)
{
    const char *p = str;
    unsigned long long magnitude;
    char *end;
    int negative = 0;

    if (!str) {
        if (endptr)
            *endptr = NULL;
        errno = EINVAL;
        return 0;
    }

    while (isspace((unsigned char)*p))
        ++p;
    if (*p == '+' || *p == '-') {
        negative = (*p++ == '-');
    }

    magnitude = strtoull(p, &end, base);
    if (end == p) {
        if (endptr)
            *endptr = (char *)str;
        return 0;
    }

    if (errno == ERANGE || magnitude > (unsigned long long)ULONG_MAX) {
        errno = ERANGE;
        if (endptr)
            *endptr = end;
        return ULONG_MAX;
    }

    if (endptr)
        *endptr = end;

    return negative ? 0ul - (unsigned long)magnitude : (unsigned long)magnitude;
}

long long strtoll(const char *str, char **endptr, int base)
{
    const char *p = str;
    unsigned long long magnitude;
    unsigned long long limit = (unsigned long long)LLONG_MAX;
    char *end;
    int negative = 0;

    if (!str) {
        if (endptr)
            *endptr = NULL;
        errno = EINVAL;
        return 0;
    }

    while (isspace((unsigned char)*p))
        ++p;
    if (*p == '+' || *p == '-')
        negative = (*p++ == '-');

    if (negative)
        limit += 1ull;

    magnitude = strtoull(p, &end, base);
    if (end == p) {
        if (endptr)
            *endptr = (char *)str;
        return 0;
    }

    if (errno == ERANGE || magnitude > limit) {
        errno = ERANGE;
        if (endptr)
            *endptr = end;
        return negative ? LLONG_MIN : LLONG_MAX;
    }

    if (endptr)
        *endptr = end;

    if (!negative)
        return (long long)magnitude;
    if (magnitude == limit)
        return LLONG_MIN;
    return -(long long)magnitude;
}

long strtol(const char *str, char **endptr, int base)
{
    long long value = strtoll(str, endptr, base);

    if (value > (long long)LONG_MAX) {
        errno = ERANGE;
        return LONG_MAX;
    }
    if (value < (long long)LONG_MIN) {
        errno = ERANGE;
        return LONG_MIN;
    }
    return (long)value;
}

int atoi(const char *str)
{
    return (int)strtol(str, NULL, 10);
}

long atol(const char *str)
{
    return strtol(str, NULL, 10);
}

int abs(int value)
{
    return value < 0 ? -value : value;
}

long labs(long value)
{
    return value < 0 ? -value : value;
}

static void qsort_sift(unsigned char *base, size_t root, size_t end, size_t size, int (*compare)(const void *, const void *))
{
    while (root <= (end - 1u) / 2u && end > 0) {
        size_t child = root * 2u + 1u;
        size_t largest = root;

        if (compare(base + largest * size, base + child * size) < 0)
            largest = child;
        if (child + 1u <= end && compare(base + largest * size, base + (child + 1u) * size) < 0)
            largest = child + 1u;
        if (largest == root)
            return;

        for (size_t i = 0; i < size; ++i) {
            unsigned char tmp = base[root * size + i];
            base[root * size + i] = base[largest * size + i];
            base[largest * size + i] = tmp;
        }

        root = largest;
    }
}

void qsort(void *base_ptr, size_t count, size_t size, int (*compare)(const void *, const void *))
{
    unsigned char *base = (unsigned char *)base_ptr;
    size_t start;
    size_t end;

    if (!base || !compare || size == 0 || count < 2)
        return;

    start = (count - 2u) / 2u + 1u;
    while (start) {
        --start;
        qsort_sift(base, start, count - 1u, size, compare);
    }

    end = count - 1u;
    while (end) {
        for (size_t i = 0; i < size; ++i) {
            unsigned char tmp = base[i];
            base[i] = base[end * size + i];
            base[end * size + i] = tmp;
        }
        --end;
        if (end)
            qsort_sift(base, 0, end, size, compare);
    }
}

void *bsearch(const void *key, const void *base_ptr, size_t count, size_t size, int (*compare)(const void *, const void *))
{
    const unsigned char *base = (const unsigned char *)base_ptr;
    size_t low = 0;
    size_t high = count;

    if (!key || !base || !compare || !size)
        return NULL;

    while (low < high) {
        size_t mid = low + (high - low) / 2u;
        const void *item = base + mid * size;
        int result = compare(key, item);

        if (result < 0)
            high = mid;
        else if (result > 0)
            low = mid + 1u;
        else
            return (void *)item;
    }

    return NULL;
}

char *getenv(const char *name)
{
    (void)name;
    return NULL;
}

char *strerror(int error_number)
{
    switch (error_number) {
        case 0: return "No error";
        case EPERM: return "Operation not permitted";
        case ENOENT: return "No such file or directory";
        case EINTR: return "Interrupted operation";
        case EIO: return "Input/output error";
        case EBADF: return "Bad file descriptor";
        case EAGAIN: return "Resource temporarily unavailable";
        case ENOMEM: return "Out of memory";
        case EACCES: return "Permission denied";
        case EFAULT: return "Bad address";
        case EEXIST: return "File exists";
        case ENOTDIR: return "Not a directory";
        case EISDIR: return "Is a directory";
        case EINVAL: return "Invalid argument";
        case ENOSPC: return "No space left";
        case ERANGE: return "Result out of range";
        case ENOSYS: return "Function not implemented";
        case EFBIG: return "File too large";
        case ENAMETOOLONG: return "Name too long";
        default: return "Unknown error";
    }
}

void exit(int status)
{
    (void)status;
    abort();
}

void _Exit(int status)
{
    (void)status;
    abort();
}

void abort(void)
{
    __asm__ volatile ("cli");
    for (;;) 
        __asm__ volatile ("hlt");
}

int rand(void)
{
    random_seed = random_seed * 1103515245u + 12345u;
    return (int)((random_seed >> 16) & RAND_MAX);
}

void srand(unsigned int seed)
{
    random_seed = seed ? seed : 1u;
}

long double strtold(const char *str, char **endptr)
{
    const char *p = str;
    long double value = 0.0L;
    long double fraction = 1.0L;
    int sign = 1;
    int base = 10;
    int exponent = 0;
    int exponent_sign = 1;
    int digits = 0;
    int fractional = 0;

    while (isspace((unsigned char)*p))
        ++p;

    if (*p == '+' || *p == '-') {
        if (*p == '-')
            sign = -1;
        ++p;
    }

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    }

    for (;;) {
        int digit;
        if (*p >= '0' && *p <= '9')
            digit = *p - '0';
        else if (*p >= 'a' && *p <= 'f')
            digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F')
            digit = *p - 'A' + 10;
        else if (*p == '.' && !fractional) {
            fractional = 1;
            ++p;
            continue;
        } else {
            break;
        }

        if (digit >= base)
            break;

        value = value * (long double)base + (long double)digit;
        if (fractional)
            fraction *= (long double)base;
        ++digits;
        ++p;
    }

    if (!digits) {
        if (endptr)
            *endptr = (char *)str;
        return 0.0L;
    }

    if (fraction > 1.0L)
        value /= fraction;

    if ((base == 10 && (*p == 'e' || *p == 'E')) ||
        (base == 16 && (*p == 'p' || *p == 'P'))) {
        const char *exp_start = p++;
        if (*p == '+' || *p == '-') {
            if (*p == '-')
                exponent_sign = -1;
            ++p;
        }
        if (!isdigit((unsigned char)*p)) {
            p = exp_start;
        } else {
            while (isdigit((unsigned char)*p)) {
                if (exponent < 10000) {
                    exponent = exponent * 10 + (*p - '0');
                    if (exponent > 5000)
                        exponent = 5000;
                }
                ++p;
            }
        }
    }

    exponent *= exponent_sign;
    if (exponent > 5000)
        exponent = 5000;
    if (exponent < -5000)
        exponent = -5000;
    if (base == 16) {
        while (exponent > 0) {
            value *= 2.0L;
            --exponent;
        }
        while (exponent < 0) {
            value /= 2.0L;
            ++exponent;
        }
    } else {
        while (exponent > 0) {
            value *= 10.0L;
            --exponent;
        }
        while (exponent < 0) {
            value /= 10.0L;
            ++exponent;
        }
    }

    if (endptr)
        *endptr = (char *)p;
    return sign < 0 ? -value : value;
}

float strtof(const char *str, char **endptr)
{
    return (float)strtold(str, endptr);
}

int isinf(double value)
{
    return __builtin_isinf(value);
}

int isnan(double value)
{
    return __builtin_isnan(value);
}

char *realpath(const char *path, char *resolved_path)
{
    char temporary[512];
    char *result;
    size_t length;
    size_t output_length = 1;
    size_t segment_start[128];
    size_t segment_count = 0;
    const char *source;

    if (!path || !*path) {
        errno = EINVAL;
        return NULL;
    }

    if (path[0] == '/') {
        source = path;
    } else {
        temporary[0] = '/';
        temporary[1] = '\0';
        if (strlen(path) + 2u >= sizeof(temporary)) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        strcat(temporary, path);
        source = temporary;
    }

    length = strlen(source);
    if (length >= sizeof(temporary)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    if (source != temporary)
        memcpy(temporary, source, length + 1u);

    result = resolved_path;
    if (!result) {
        result = (char *)malloc(512u);
        if (!result) {
            errno = ENOMEM;
            return NULL;
        }
    }

    result[0] = '/';
    result[1] = '\0';

    for (size_t i = 1; i < length;) {
        size_t start;
        size_t segment_length;
        while (temporary[i] == '/')
            ++i;
        if (!temporary[i])
            break;
        start = i;
        while (temporary[i] && temporary[i] != '/')
            ++i;
        segment_length = i - start;

        if (segment_length == 1u && temporary[start] == '.')
            continue;
        if (segment_length == 2u && temporary[start] == '.' && temporary[start + 1u] == '.') {
            if (segment_count) {
                output_length = segment_start[--segment_count];
                result[output_length] = '\0';
            }
            continue;
        }

        if (output_length > 1u)
            result[output_length++] = '/';
        if (segment_count < 128u)
            segment_start[segment_count++] = output_length > 1u ? output_length - 1u : 1u;
        memcpy(result + output_length, temporary + start, segment_length);
        output_length += segment_length;
        result[output_length] = '\0';
    }

    return result;
}


double strtod(const char *str, char **endptr)
{
    return (double)strtold(str, endptr);
}

long double ldexpl(long double value, int exponent)
{
    if (exponent > 5000)
        exponent = 5000;
    if (exponent < -5000)
        exponent = -5000;
    while (exponent > 0) {
        value *= 2.0L;
        --exponent;
    }
    while (exponent < 0) {
        value /= 2.0L;
        ++exponent;
    }
    return value;
}
