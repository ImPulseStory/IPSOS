#include "../include/stdio.h"
#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/errno.h"
#include "../include/fcntl.h"
#include "../include/unistd.h"
#include "../include/task.h"
#include "../include/ipsfs.h"
#include <stdint.h>
#include <stddef.h>

#define IPS_FILE_MAGIC 0x49504649u
#define IPS_FILE_LIMIT (BLOCK_SIZE * DIRECT_BLOCKS)
#define IPS_F_READ 0x01u
#define IPS_F_WRITE 0x02u
#define IPS_F_APPEND 0x04u
#define IPS_F_CONSOLE_IN 0x08u
#define IPS_F_CONSOLE_OUT 0x10u
#define IPS_F_DIRTY 0x20u
#define IPS_F_EOF 0x40u
#define IPS_F_ERROR 0x80u
#define IPS_F_PUSHBACK 0x100u
#define IPS_FD_COUNT 16

typedef struct ips_file {
    uint32_t magic;
    uint32_t flags;
    uint32_t size;
    uint32_t position;
    uint32_t capacity;
    int pushback;
    int descriptor;
    char path[256];
    uint8_t *data;
    struct ips_file *next;
} ips_file_t;

static ips_file_t stdin_object = { IPS_FILE_MAGIC, IPS_F_READ | IPS_F_CONSOLE_IN, 0, 0, 0, -1, 0, {0}, NULL, NULL };
static ips_file_t stdout_object = { IPS_FILE_MAGIC, IPS_F_WRITE | IPS_F_CONSOLE_OUT, 0, 0, 0, -1, 1, {0}, NULL, NULL };
static ips_file_t stderr_object = { IPS_FILE_MAGIC, IPS_F_WRITE | IPS_F_CONSOLE_OUT, 0, 0, 0, -1, 2, {0}, NULL, NULL };

FILE *stdin = (FILE *)&stdin_object;
FILE *stdout = (FILE *)&stdout_object;
FILE *stderr = (FILE *)&stderr_object;

static FILE *fd_table[IPS_FD_COUNT];
static ips_file_t *stream_list;
static char current_directory[256] = "/";

extern void term_print_char(char c);
extern int get_key(void);

static void stream_link(ips_file_t *file)
{
    file->next = stream_list;
    stream_list = file;
}

static void stream_unlink(ips_file_t *file)
{
    ips_file_t **link = &stream_list;

    while (*link) {
        if (*link == file) {
            *link = file->next;
            file->next = NULL;
            return;
        }
        link = &(*link)->next;
    }
}

static int file_valid(FILE *stream)
{
    ips_file_t *file = (ips_file_t *)stream;
    return file && file->magic == IPS_FILE_MAGIC;
}

static int normalize_path(const char *path, char *out, size_t capacity)
{
    size_t marks[128];
    size_t depth = 0;
    size_t out_length = 1;

    if (!path || !out || capacity < 2 || path[0] != '/')
        return -1;

    out[0] = '/';
    out[1] = '\0';

    while (*path == '/')
        ++path;

    while (*path) {
        char component[256];
        size_t component_length = 0;

        while (*path && *path != '/') {
            if (component_length + 1u >= sizeof(component))
                return -1;
            component[component_length++] = *path++;
        }
        component[component_length] = '\0';

        while (*path == '/')
            ++path;

        if (!component_length || (component_length == 1u && component[0] == '.'))
            continue;

        if (component_length == 2u && component[0] == '.' && component[1] == '.') {
            if (depth) {
                out_length = marks[--depth];
                out[out_length] = '\0';
            }
            continue;
        }

        if (depth >= sizeof(marks) / sizeof(marks[0]))
            return -1;

        marks[depth++] = out_length;
        if (out_length > 1u) {
            if (out_length + 1u >= capacity)
                return -1;
            out[out_length++] = '/';
        }
        if (out_length + component_length >= capacity)
            return -1;
        memcpy(out + out_length, component, component_length);
        out_length += component_length;
        out[out_length] = '\0';
    }

    return 0;
}

static int make_path(const char *path, char *out, size_t capacity)
{
    char combined[256];
    size_t base_length;
    size_t path_length;

    if (!path || !*path || !out || capacity < 2u)
        return -1;

    if (path[0] == '/') {
        path_length = strlen(path);
        if (path_length >= sizeof(combined))
            return -1;
        memcpy(combined, path, path_length + 1u);
    } else {
        base_length = strlen(current_directory);
        path_length = strlen(path);
        if (base_length + path_length + (base_length > 1u ? 2u : 1u) > sizeof(combined))
            return -1;
        memcpy(combined, current_directory, base_length);
        if (base_length > 1u)
            combined[base_length++] = '/';
        memcpy(combined + base_length, path, path_length + 1u);
    }

    return normalize_path(combined, out, capacity);
}

static void console_write(const char *data, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        term_print_char(data[i]);
}

static int file_flush(FILE *stream)
{
    ips_file_t *file = (ips_file_t *)stream;

    if (!file_valid(stream)) {
        errno = EBADF;
        return EOF;
    }

    if (!(file->flags & IPS_F_DIRTY))
        return 0;

    if (!file->path[0] || ips_write_file(file->path, file->data, file->size) < 0) {
        file->flags |= IPS_F_ERROR;
        errno = EIO;
        return EOF;
    }

    file->flags &= ~IPS_F_DIRTY;
    return 0;
}

FILE *fopen(const char *path, const char *mode)
{
    char full_path[256];
    ips_file_t *file;
    uint32_t file_size = 0;
    uint32_t flags = 0;
    int exists;
    int create_mode;
    int truncate_mode;
    size_t path_len;

    if (!path || !mode || !mode[0] || make_path(path, full_path, sizeof(full_path)) != 0) {
        errno = EINVAL;
        return NULL;
    }

    if (mode[0] == 'r')
        flags |= IPS_F_READ;
    else if (mode[0] == 'w')
        flags |= IPS_F_WRITE;
    else if (mode[0] == 'a')
        flags |= IPS_F_WRITE | IPS_F_APPEND;
    else {
        errno = EINVAL;
        return NULL;
    }

    if (strchr(mode, '+'))
        flags |= IPS_F_READ | IPS_F_WRITE;

    create_mode = mode[0] == 'w' || mode[0] == 'a';
    truncate_mode = mode[0] == 'w';
    path_len = strlen(full_path);
    if (path_len >= 256u) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    exists = ips_resolve_path(full_path) >= 0;
    if (!exists && !create_mode) {
        errno = ENOENT;
        return NULL;
    }

    if (!exists && ips_create_file(full_path) != 0) {
        errno = EIO;
        return NULL;
    }

    file = (ips_file_t *)malloc(sizeof(ips_file_t));
    if (!file) {
        errno = ENOMEM;
        return NULL;
    }

    memset(file, 0, sizeof(*file));
    file->data = (uint8_t *)malloc(IPS_FILE_LIMIT);
    if (!file->data) {
        free(file);
        errno = ENOMEM;
        return NULL;
    }

    file->magic = IPS_FILE_MAGIC;
    file->flags = flags;
    file->capacity = IPS_FILE_LIMIT;
    file->descriptor = -1;
    memcpy(file->path, full_path, path_len + 1u);

    if (ips_read_file(full_path, file->data, file->capacity, &file_size) == 0) {
        file->size = file_size;
    } else if (!create_mode) {
        free(file->data);
        free(file);
        errno = EIO;
        return NULL;
    }

    if (truncate_mode) {
        if (ips_write_file(full_path, NULL, 0) < 0) {
            free(file->data);
            free(file);
            errno = EIO;
            return NULL;
        }
        file->size = 0;
    }

    if (flags & IPS_F_APPEND)
        file->position = file->size;

    stream_link(file);
    return (FILE *)file;
}

int fclose(FILE *stream)
{
    ips_file_t *file = (ips_file_t *)stream;
    int result;

    if (!file_valid(stream) || stream == stdin || stream == stdout || stream == stderr) {
        errno = EBADF;
        return EOF;
    }

    result = file_flush(stream);

    if (file->descriptor >= 3 && file->descriptor < IPS_FD_COUNT)
        fd_table[file->descriptor] = NULL;

    stream_unlink(file);
    if (file->data)
        free(file->data);

    file->magic = 0;
    free(file);
    return result;
}

int fflush(FILE *stream)
{
    if (!stream) {
        int result = 0;
        if (file_flush(stdout) == EOF)
            result = EOF;
        if (file_flush(stderr) == EOF)
            result = EOF;
        for (ips_file_t *file = stream_list; file; file = file->next) {
            if (file_flush((FILE *)file) == EOF)
                result = EOF;
        }
        return result;
    }

    return file_flush(stream);
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream)
{
    ips_file_t *file = (ips_file_t *)stream;
    size_t total;
    size_t available = 0;

    if (!file_valid(stream) || (!ptr && size && count) || !(file->flags & IPS_F_READ)) {
        if (file_valid(stream))
            file->flags |= IPS_F_ERROR;
        errno = EBADF;
        return 0;
    }

    if (!size || !count)
        return 0;
    if (count > SIZE_MAX / size) {
        errno = EINVAL;
        return 0;
    }

    total = size * count;

    if (file->flags & IPS_F_PUSHBACK) {
        ((uint8_t *)ptr)[available++] = (uint8_t)file->pushback;
        file->flags &= ~IPS_F_PUSHBACK;
    }

    if (file->flags & IPS_F_CONSOLE_IN) {
        uint8_t *out = (uint8_t *)ptr;
        for (size_t i = available; i < total; ++i) {
            int key;
            do {
                key = get_key();
                if (!key)
                    __asm__ volatile ("hlt");
            } while (!key);
            out[i] = (uint8_t)key;
        }
        return count;
    }

    {
        size_t remaining = total - available;
        size_t file_available = file->position < file->size ? file->size - file->position : 0;
        if (remaining > file_available)
            remaining = file_available;
        if (remaining)
            memcpy((uint8_t *)ptr + available, file->data + file->position, remaining);
        file->position += (uint32_t)remaining;
        available += remaining;
    }

    if (available < total)
        file->flags |= IPS_F_EOF;

    return available / size;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream)
{
    ips_file_t *file = (ips_file_t *)stream;
    size_t total;
    size_t writable;

    if (!file_valid(stream) || (!ptr && size && count) || !(file->flags & IPS_F_WRITE)) {
        if (file_valid(stream))
            file->flags |= IPS_F_ERROR;
        errno = EBADF;
        return 0;
    }

    if (!size || !count)
        return 0;
    if (count > SIZE_MAX / size) {
        errno = EINVAL;
        return 0;
    }

    total = size * count;

    if (file->flags & IPS_F_CONSOLE_OUT) {
        console_write((const char *)ptr, total);
        return count;
    }

    if (file->flags & IPS_F_APPEND)
        file->position = file->size;

    if (file->position >= file->capacity)
        return 0;

    writable = file->capacity - file->position;
    if (total > writable)
        total = writable;

    if (file->position > file->size)
        memset(file->data + file->size, 0, file->position - file->size);

    memcpy(file->data + file->position, ptr, total);
    file->position += (uint32_t)total;
    if (file->position > file->size)
        file->size = file->position;
    file->flags |= IPS_F_DIRTY;

    return total / size;
}

int fseek(FILE *stream, long offset, int origin)
{
    ips_file_t *file = (ips_file_t *)stream;
    long long base;
    long long target;

    if (!file_valid(stream) || (file->flags & (IPS_F_CONSOLE_IN | IPS_F_CONSOLE_OUT))) {
        errno = EINVAL;
        return -1;
    }

    if (origin == SEEK_SET)
        base = 0;
    else if (origin == SEEK_CUR)
        base = file->position;
    else if (origin == SEEK_END)
        base = file->size;
    else {
        errno = EINVAL;
        return -1;
    }

    target = base + offset;
    if (target < 0 || target > file->capacity) {
        errno = EINVAL;
        return -1;
    }

    file->position = (uint32_t)target;
    file->flags &= ~(IPS_F_EOF | IPS_F_PUSHBACK);
    return 0;
}

long ftell(FILE *stream)
{
    ips_file_t *file = (ips_file_t *)stream;

    if (!file_valid(stream)) {
        errno = EBADF;
        return -1L;
    }

    return (long)file->position;
}

void rewind(FILE *stream)
{
    if (!file_valid(stream))
        return;
    fseek(stream, 0, SEEK_SET);
    clearerr(stream);
}

int feof(FILE *stream)
{
    return file_valid(stream) && ((((ips_file_t *)stream)->flags & IPS_F_EOF) != 0);
}

int ferror(FILE *stream)
{
    return !file_valid(stream) || ((((ips_file_t *)stream)->flags & IPS_F_ERROR) != 0);
}

void clearerr(FILE *stream)
{
    if (file_valid(stream))
        ((ips_file_t *)stream)->flags &= ~(IPS_F_EOF | IPS_F_ERROR);
}

int fgetc(FILE *stream)
{
    ips_file_t *file = (ips_file_t *)stream;
    unsigned char value;

    if (!file_valid(stream)) {
        errno = EBADF;
        return EOF;
    }

    if (file->flags & IPS_F_PUSHBACK) {
        file->flags &= ~IPS_F_PUSHBACK;
        return file->pushback;
    }

    if (fread(&value, 1, 1, stream) != 1)
        return EOF;

    return value;
}

int getc(FILE *stream)
{
    return fgetc(stream);
}

int getchar(void)
{
    return fgetc(stdin);
}

int ungetc(int c, FILE *stream)
{
    ips_file_t *file = (ips_file_t *)stream;

    if (!file_valid(stream) || c == EOF || (file->flags & IPS_F_PUSHBACK))
        return EOF;

    file->pushback = (unsigned char)c;
    file->flags |= IPS_F_PUSHBACK;
    file->flags &= ~IPS_F_EOF;
    return (unsigned char)c;
}

int fputc(int c, FILE *stream)
{
    unsigned char value = (unsigned char)c;
    return fwrite(&value, 1, 1, stream) == 1 ? value : EOF;
}

int putc(int c, FILE *stream)
{
    return fputc(c, stream);
}

int putchar(int c)
{
    return fputc(c, stdout);
}

char *fgets(char *str, int count, FILE *stream)
{
    int i = 0;

    if (!str || count <= 0 || !file_valid(stream))
        return NULL;

    while (i < count - 1) {
        int c = fgetc(stream);
        if (c == EOF)
            break;
        str[i++] = (char)c;
        if (c == '\n')
            break;
    }

    if (!i)
        return NULL;

    str[i] = '\0';
    return str;
}

int fputs(const char *str, FILE *stream)
{
    size_t len;

    if (!str || !file_valid(stream)) {
        errno = EINVAL;
        return EOF;
    }

    len = strlen(str);
    return fwrite(str, 1, len, stream) == len ? 0 : EOF;
}

int puts(const char *str)
{
    if (fputs(str, stdout) == EOF || fputc('\n', stdout) == EOF)
        return EOF;
    return 0;
}

int remove(const char *path)
{
    char full_path[256];

    if (make_path(path, full_path, sizeof(full_path)) != 0) {
        errno = EINVAL;
        return -1;
    }

    if (ips_delete(full_path) != 0) {
        errno = ENOENT;
        return -1;
    }

    return 0;
}

int rename(const char *old_path, const char *new_path)
{
    char old_full[256];
    char new_full[256];
    uint8_t *buffer;
    uint32_t size = 0;

    if (make_path(old_path, old_full, sizeof(old_full)) != 0 || make_path(new_path, new_full, sizeof(new_full)) != 0) {
        errno = EINVAL;
        return -1;
    }

    buffer = (uint8_t *)malloc(IPS_FILE_LIMIT);
    if (!buffer) {
        errno = ENOMEM;
        return -1;
    }

    if (ips_read_file(old_full, buffer, IPS_FILE_LIMIT, &size) != 0) {
        free(buffer);
        errno = ENOENT;
        return -1;
    }

    if (ips_resolve_path(new_full) >= 0 || ips_create_file(new_full) != 0) {
        free(buffer);
        errno = EEXIST;
        return -1;
    }

    if (ips_write_file(new_full, buffer, size) < 0 || ips_delete(old_full) != 0) {
        free(buffer);
        errno = EIO;
        return -1;
    }

    free(buffer);
    return 0;
}

void perror(const char *str)
{
    if (str && *str) {
        fputs(str, stderr);
        fputs(": ", stderr);
    }
    fputs(strerror(errno), stderr);
    fputc('\n', stderr);
}

int setvbuf(FILE *stream, char *buffer, int mode, size_t size)
{
    (void)buffer;
    (void)mode;
    (void)size;
    if (!file_valid(stream)) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

void setbuf(FILE *stream, char *buffer)
{
    setvbuf(stream, buffer, 0, BUFSIZ);
}

int fileno(FILE *stream)
{
    if (!file_valid(stream)) {
        errno = EBADF;
        return -1;
    }
    return ((ips_file_t *)stream)->descriptor;
}

static void format_put(char *out, size_t capacity, size_t *count, char value)
{
    if (capacity && *count < capacity - 1u)
        out[*count] = value;
    ++*count;
}

static uint32_t format_div_small(unsigned long long value, uint32_t divisor, unsigned long long *quotient)
{
    uint32_t high = (uint32_t)(value >> 32);
    uint32_t low = (uint32_t)value;
    uint32_t quotient_high = high / divisor;
    uint32_t remainder = high % divisor;

    __asm__ volatile ("divl %2" : "+a"(low), "+d"(remainder) : "rm"(divisor));
    *quotient = ((unsigned long long)quotient_high << 32) | low;
    return remainder;
}

static size_t format_unsigned(char *buffer, unsigned long long value, unsigned int base, int upper)
{
    char temp[32];
    size_t len = 0;
    const char *digits = upper ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";

    do {
        unsigned long long quotient;
        uint32_t remainder = format_div_small(value, base, &quotient);
        temp[len++] = digits[remainder];
        value = quotient;
    } while (value && len < sizeof(temp));

    for (size_t i = 0; i < len; ++i)
        buffer[i] = temp[len - i - 1u];

    return len;
}

int vsnprintf(char *out, size_t capacity, const char *format, va_list args)
{
    size_t count = 0;

    if (!format || (!out && capacity)) {
        errno = EINVAL;
        return -1;
    }

    while (*format) {
        int left = 0;
        int plus = 0;
        int space = 0;
        int alternate = 0;
        int zero = 0;
        int width = 0;
        int precision = -1;
        int length = 0;
        char conversion;
        char number[32];
        size_t number_len = 0;
        size_t prefix_len = 0;
        char prefix[2];
        size_t zeroes;
        size_t padding;
        int negative = 0;
        unsigned long long value = 0;
        unsigned int base = 10;
        int is_number = 0;
        int upper = 0;

        if (*format != '%') {
            format_put(out, capacity, &count, *format++);
            continue;
        }

        ++format;
        if (*format == '%') {
            format_put(out, capacity, &count, '%');
            ++format;
            continue;
        }

        for (;;) {
            if (*format == '-') left = 1;
            else if (*format == '+') plus = 1;
            else if (*format == ' ') space = 1;
            else if (*format == '#') alternate = 1;
            else if (*format == '0') zero = 1;
            else break;
            ++format;
        }

        if (*format == '*') {
            width = va_arg(args, int);
            if (width < 0) {
                left = 1;
                width = -width;
            }
            ++format;
        } else {
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format++ - '0');
                if (width > 4096)
                    width = 4096;
            }
        }

        if (*format == '.') {
            ++format;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(args, int);
                if (precision < 0)
                    precision = -1;
                ++format;
            } else {
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format++ - '0');
                    if (precision > 4096)
                        precision = 4096;
                }
            }
        }

        if (*format == 'h') {
            ++format;
            length = 1;
            if (*format == 'h') {
                ++format;
                length = 2;
            }
        } else if (*format == 'l') {
            ++format;
            length = 3;
            if (*format == 'l') {
                ++format;
                length = 4;
            }
        } else if (*format == 'z' || *format == 't') {
            ++format;
            length = 5;
        } else if (*format == 'j') {
            ++format;
            length = 4;
        }

        conversion = *format ? *format++ : '\0';

        if (conversion == 's') {
            const char *text = va_arg(args, const char *);
            size_t len = text ? strlen(text) : 6u;
            if (!text)
                text = "(null)";
            if (precision >= 0 && len > (size_t)precision)
                len = (size_t)precision;
            padding = width > (int)len ? (size_t)width - len : 0;
            if (!left)
                for (size_t i = 0; i < padding; ++i)
                    format_put(out, capacity, &count, ' ');
            for (size_t i = 0; i < len; ++i)
                format_put(out, capacity, &count, text[i]);
            if (left)
                for (size_t i = 0; i < padding; ++i)
                    format_put(out, capacity, &count, ' ');
            continue;
        }

        if (conversion == 'c') {
            char c = (char)va_arg(args, int);
            padding = width > 1 ? (size_t)width - 1u : 0;
            if (!left)
                for (size_t i = 0; i < padding; ++i)
                    format_put(out, capacity, &count, ' ');
            format_put(out, capacity, &count, c);
            if (left)
                for (size_t i = 0; i < padding; ++i)
                    format_put(out, capacity, &count, ' ');
            continue;
        }

        if (conversion == 'd' || conversion == 'i') {
            long long signed_value;
            if (length == 4)
                signed_value = va_arg(args, long long);
            else if (length == 3 || length == 5)
                signed_value = va_arg(args, long);
            else
                signed_value = va_arg(args, int);
            negative = signed_value < 0;
            value = negative ? (unsigned long long)(-(signed_value + 1ll)) + 1ull : (unsigned long long)signed_value;
            is_number = 1;
        } else if (conversion == 'u' || conversion == 'o' || conversion == 'x' || conversion == 'X' || conversion == 'p') {
            if (conversion == 'o')
                base = 8;
            else if (conversion == 'x' || conversion == 'X' || conversion == 'p')
                base = 16;
            upper = conversion == 'X';
            if (conversion == 'p')
                value = (uintptr_t)va_arg(args, void *);
            else if (length == 4)
                value = va_arg(args, unsigned long long);
            else if (length == 3 || length == 5)
                value = va_arg(args, unsigned long);
            else
                value = va_arg(args, unsigned int);
            is_number = 1;
        } else if (conversion == 'n') {
            if (length == 4) {
                long long *p = va_arg(args, long long *);
                if (p) *p = (long long)count;
            } else if (length == 3 || length == 5) {
                long *p = va_arg(args, long *);
                if (p) *p = (long)count;
            } else {
                int *p = va_arg(args, int *);
                if (p) *p = (int)count;
            }
            continue;
        } else if (conversion == '\0') {
            break;
        } else {
            format_put(out, capacity, &count, '%');
            format_put(out, capacity, &count, conversion);
            continue;
        }

        if (is_number) {
            if (value || precision != 0)
                number_len = format_unsigned(number, value, base, upper);

            if (negative) {
                prefix[prefix_len++] = '-';
            } else if (plus && (conversion == 'd' || conversion == 'i')) {
                prefix[prefix_len++] = '+';
            } else if (space && (conversion == 'd' || conversion == 'i')) {
                prefix[prefix_len++] = ' ';
            }

            if ((alternate && value && base == 16) || conversion == 'p') {
                prefix[prefix_len++] = '0';
                if (prefix_len < sizeof(prefix))
                    prefix[prefix_len++] = upper ? 'X' : 'x';
            } else if (alternate && value && base == 8 && (!number_len || number[0] != '0')) {
                prefix[prefix_len++] = '0';
            }

            zeroes = precision > (int)number_len ? (size_t)precision - number_len : 0;
            if (precision >= 0)
                zero = 0;
            padding = width > (int)(prefix_len + zeroes + number_len) ? (size_t)width - prefix_len - zeroes - number_len : 0;

            if (!left && !zero)
                for (size_t i = 0; i < padding; ++i)
                    format_put(out, capacity, &count, ' ');
            for (size_t i = 0; i < prefix_len; ++i)
                format_put(out, capacity, &count, prefix[i]);
            if (!left && zero)
                for (size_t i = 0; i < padding; ++i)
                    format_put(out, capacity, &count, '0');
            for (size_t i = 0; i < zeroes; ++i)
                format_put(out, capacity, &count, '0');
            for (size_t i = 0; i < number_len; ++i)
                format_put(out, capacity, &count, number[i]);
            if (left)
                for (size_t i = 0; i < padding; ++i)
                    format_put(out, capacity, &count, ' ');
        }
    }

    if (capacity)
        out[count < capacity ? count : capacity - 1u] = '\0';

    if (count > 0x7FFFFFFFu)
        return -1;

    return (int)count;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

int sprintf(char *str, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = vsnprintf(str, SIZE_MAX, format, args);
    va_end(args);
    return result;
}

int vsprintf(char *str, const char *format, va_list args)
{
    return vsnprintf(str, SIZE_MAX, format, args);
}

int vfprintf(FILE *stream, const char *format, va_list args)
{
    va_list copy;
    char *buffer;
    int result;
    size_t output_size;

    if (!file_valid(stream) || !format) {
        errno = EINVAL;
        return -1;
    }

    va_copy(copy, args);
    result = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    if (result < 0)
        return result;

    buffer = (char *)malloc((size_t)result + 1u);
    if (!buffer) {
        errno = ENOMEM;
        return -1;
    }

    va_copy(copy, args);
    result = vsnprintf(buffer, (size_t)result + 1u, format, copy);
    va_end(copy);

    if (result < 0) {
        free(buffer);
        return result;
    }

    output_size = (size_t)result;
    if (fwrite(buffer, 1, output_size, stream) != output_size) {
        free(buffer);
        return EOF;
    }

    free(buffer);
    return result;
}

int fprintf(FILE *stream, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = vfprintf(stream, format, args);
    va_end(args);
    return result;
}

int vprintf(const char *format, va_list args)
{
    return vfprintf(stdout, format, args);
}

int printf(const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = vfprintf(stdout, format, args);
    va_end(args);
    return result;
}

static int fd_allocate(FILE *stream)
{
    for (int fd = 3; fd < IPS_FD_COUNT; ++fd) {
        if (!fd_table[fd]) {
            fd_table[fd] = stream;
            ((ips_file_t *)stream)->descriptor = fd;
            return fd;
        }
    }
    errno = EMFILE;
    return -1;
}

int open(const char *path, int flags, ...)
{
    const char *mode;
    FILE *stream;
    int access_mode = flags & O_ACCMODE;

    if (access_mode == O_RDONLY && (flags & O_TRUNC)) {
        errno = EINVAL;
        return -1;
    }

    if (access_mode == O_RDONLY)
        mode = "r";
    else if (access_mode == O_WRONLY)
        mode = (flags & O_APPEND) ? "a" : ((flags & O_TRUNC) ? "w" : "r+");
    else if (access_mode == O_RDWR)
        mode = (flags & O_APPEND) ? "a+" : ((flags & O_TRUNC) ? "w+" : "r+");
    else {
        errno = EINVAL;
        return -1;
    }

    {
        char full_path[256];
        if (make_path(path, full_path, sizeof(full_path)) != 0) {
            errno = EINVAL;
            return -1;
        }
        if ((flags & O_EXCL) && (flags & O_CREAT) && ips_resolve_path(full_path) >= 0) {
            errno = EEXIST;
            return -1;
        }
        if (!(flags & O_CREAT) && (flags & (O_TRUNC | O_APPEND)) && ips_resolve_path(full_path) < 0) {
            errno = ENOENT;
            return -1;
        }
    }

    stream = fopen(path, mode);
    if (!stream && (flags & O_CREAT)) {
        char full_path[256];
        if (make_path(path, full_path, sizeof(full_path)) == 0 && ips_create_file(full_path) == 0)
            stream = fopen(path, mode);
    }

    if (!stream)
        return -1;

    if (access_mode == O_RDONLY)
        ((ips_file_t *)stream)->flags &= ~IPS_F_WRITE;
    else if (access_mode == O_WRONLY)
        ((ips_file_t *)stream)->flags &= ~IPS_F_READ;

    {
        int fd = fd_allocate(stream);
        if (fd < 0)
            fclose(stream);
        return fd;
    }
}

ssize_t read(int fd, void *buffer, size_t count)
{
    FILE *stream;
    size_t result;

    if (fd == STDIN_FILENO)
        stream = stdin;
    else if (fd >= 3 && fd < IPS_FD_COUNT)
        stream = fd_table[fd];
    else {
        errno = EBADF;
        return -1;
    }

    if (!stream) {
        errno = EBADF;
        return -1;
    }

    result = fread(buffer, 1, count, stream);
    if (!result && ferror(stream))
        return -1;
    return (ssize_t)result;
}

ssize_t write(int fd, const void *buffer, size_t count)
{
    FILE *stream;
    size_t result;

    if (fd == STDOUT_FILENO)
        stream = stdout;
    else if (fd == STDERR_FILENO)
        stream = stderr;
    else if (fd >= 3 && fd < IPS_FD_COUNT)
        stream = fd_table[fd];
    else {
        errno = EBADF;
        return -1;
    }

    if (!stream) {
        errno = EBADF;
        return -1;
    }

    result = fwrite(buffer, 1, count, stream);
    if (result < count && ferror(stream))
        return -1;
    return (ssize_t)result;
}

int close(int fd)
{
    if (fd < 3 || fd >= IPS_FD_COUNT || !fd_table[fd]) {
        errno = EBADF;
        return -1;
    }

    return fclose(fd_table[fd]);
}

off_t lseek(int fd, off_t offset, int whence)
{
    FILE *stream;

    if (fd < 3 || fd >= IPS_FD_COUNT || !fd_table[fd]) {
        errno = EBADF;
        return -1;
    }

    stream = fd_table[fd];
    if (fseek(stream, (long)offset, whence) != 0)
        return -1;

    return (off_t)ftell(stream);
}

int unlink(const char *path)
{
    return remove(path);
}

int access(const char *path, int mode)
{
    char full_path[256];
    int inode;
    (void)mode;

    if (make_path(path, full_path, sizeof(full_path)) != 0) {
        errno = EINVAL;
        return -1;
    }

    inode = ips_resolve_path(full_path);
    if (inode < 0) {
        errno = ENOENT;
        return -1;
    }

    return 0;
}

int isatty(int fd)
{
    return fd >= STDIN_FILENO && fd <= STDERR_FILENO;
}

char *getcwd(char *buffer, size_t size)
{
    size_t len = strlen(current_directory);

    if (!buffer || size <= len) {
        errno = ERANGE;
        return NULL;
    }

    memcpy(buffer, current_directory, len + 1u);
    return buffer;
}

int chdir(const char *path)
{
    char full_path[256];
    char listing[512];
    int result;

    if (make_path(path, full_path, sizeof(full_path)) != 0) {
        errno = EINVAL;
        return -1;
    }

    if (ips_resolve_path(full_path) < 0) {
        errno = ENOENT;
        return -1;
    }

    result = ips_list(full_path, listing, sizeof(listing));
    if (result == -2) {
        errno = ENOTDIR;
        return -1;
    }

    memcpy(current_directory, full_path, strlen(full_path) + 1u);
    return 0;
}

int usleep(unsigned int microseconds)
{
    uint32_t milliseconds;
    if (!microseconds)
        return 0;
    milliseconds = microseconds / 1000u;
    if (microseconds % 1000u)
        ++milliseconds;
    task_sleep(milliseconds);
    return 0;
}

unsigned int sleep(unsigned int seconds)
{
    const unsigned int max_seconds = UINT32_MAX / 1000u;
    while (seconds > max_seconds) {
        task_sleep(max_seconds * 1000u);
        seconds -= max_seconds;
    }
    if (seconds)
        task_sleep(seconds * 1000u);
    return 0;
}


FILE *fdopen(int fd, const char *mode)
{
    ips_file_t *file;
    if (fd == STDIN_FILENO)
        return stdin;
    if (fd == STDOUT_FILENO)
        return stdout;
    if (fd == STDERR_FILENO)
        return stderr;
    if (fd < 3 || fd >= IPS_FD_COUNT || !fd_table[fd] || !mode) {
        errno = EBADF;
        return NULL;
    }
    file = (ips_file_t *)fd_table[fd];
    if (mode[0] == 'r')
        file->flags = (file->flags & ~(IPS_F_WRITE | IPS_F_APPEND)) | IPS_F_READ;
    else if (mode[0] == 'w')
        file->flags = (file->flags & ~(IPS_F_READ | IPS_F_APPEND)) | IPS_F_WRITE;
    else if (mode[0] == 'a')
        file->flags = (file->flags & ~IPS_F_READ) | IPS_F_WRITE | IPS_F_APPEND;
    else {
        errno = EINVAL;
        return NULL;
    }
    if (strchr(mode, '+'))
        file->flags |= IPS_F_READ | IPS_F_WRITE;
    return (FILE *)file;
}
