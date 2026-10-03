#include "pic.h"
#include <stdarg.h>
#include <stdbool.h>

#define WHITE_ON_BLACK 0x0F

extern void idt_init();
extern void keyboard_init();
extern char* input();

volatile unsigned short *vga = (volatile unsigned short *) 0xB8000;

int str = 0;
int prev_index = 0;
int char_index = 0;
int pr_index = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void scroll() {
    for (int i = 0; i < 80; i++) {
        vga[i] = (WHITE_ON_BLACK << 8) | ' ';
    }

    for (int y = 1; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            vga[(y-1) * 80 + x] = vga[y * 80 + x];
        }
    }
}

void print(const char *c) {
    while (*c != '\0') {
        int cursor = str * 80 + pr_index;
        if (*c == '\n') {
            str++;
            pr_index = 0;
        } else {
            vga[cursor] = (WHITE_ON_BLACK << 8) | *c;
            pr_index++;
            if (pr_index >= 80) {
                str++;
                pr_index = 0;
            }
        }
        if (str >= 25) {
            scroll();
        }
        c++;
        char_index = pr_index;
    }
}

void print_char(char c) {
    if (c == '\n') {
        str++;
        char_index = 0;
        pr_index = 0;
        return;
    }
    if (c == '\b') {
        if (char_index > 0) {
            char_index--;
            vga[str * 80 + char_index] = (WHITE_ON_BLACK << 8) | ' ';
        }
        return;
    }
    vga[str * 80 + char_index] = (WHITE_ON_BLACK << 8) | c;
    char_index++;
}

void clear() {
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            vga[y * 80 + x] = (WHITE_ON_BLACK << 8) | ' ';
        }
    }
    char_index = 0;
    str = 0;
    prev_index = 0;
}

void print_int(int n) {
    char buf[12];
    int i = 0;
    if (n == 0) { print_char('0'); return; }
    if (n < 0) { print_char('-'); n = -n; }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        print_char(buf[--i]);
    }
}

void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd': {
                    int n = va_arg(args, int);
                    print_int(n);
                    break;
                }
                case 's': {
                    char* s = va_arg(args, char*);
                    print(s);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    print_char(c);
                    break;
                }
                case '%': print_char('%'); break;
            }
        } else {
            print_char(*fmt);
        }
        fmt++;
    }
    
    va_end(args);
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

void execute(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        print("Commands aviable: \n");
        print("help - shows all comands\n");
        print("about - shows info about OS\n");
        print("clear - clear the screen\n");
    }
    if (strcmp(cmd, "about") == 0) {
        print("OS - IPSOS, hello everyone! \n");
    }
    if (strcmp(cmd, "clear") == 0) {
        clear();
    }
}

void printn(const char *c) {
    while (*c != '\0') {
        vga[str * 80 + char_index] = (WHITE_ON_BLACK << 8) | *c;
        char_index++;
        if (char_index >= 80) {
            str++;
            char_index = 0;
        }
        if (str >= 25) {
            scroll();
        }
        c++;
    }
}

void kernel_main() {
    idt_init();
    pic_remap();
    pic_unmask(0);
    pic_unmask(1);
    keyboard_init();

    clear();
    print("Hello from IPSOS! \n pidor! \n");

    outb(0x70, inb(0x70) | 0x80);
    __asm__ volatile ("sti");

    while(1) {
        print("IPSOS@user: ");
        char* cmd = input();
        execute(cmd);
    }
}