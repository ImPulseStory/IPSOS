#include "../include/vga.h"
#include "../include/libc.h"
#include "../include/sfs.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

volatile unsigned short *vga = VGA_ADDRESS;

int str = 0;
int prev_index = 0;
int char_index = 0;
int pr_index = 0;

void execute(const char *cmd) {
    uint8_t buf[512];
    char* command;
    char* args;

    parse_command((char*)cmd, &command, &args);

    if (strcmp(command, "help") == 0) {
        print("Commands aviable: \n");
        print("help - shows all comands\n");
        print("about - shows info about OS\n");
        print("clear - clear the screen\n");
    }
    if (strcmp(command, "about") == 0) {
        print("OS - IPSOS, hello everyone! \n");
    }
    if (strcmp(command, "clear") == 0) {
        clear();
    }
    if (strcmp(command, "run") == 0) {
        int pc = 0;
        char stack[100];
        bool run = true;
        int cursor = 0;

        while (run) {
            uint8_t opcode = buf[pc++];

            switch (opcode) { // PUSH (push value to stack)
                case 0x01: {
                    stack[cursor] = buf[pc++];
                    cursor++;
                    break;
                }

                case 0x30: { // PRINTC (print an char)
                    char value = stack[cursor - 1];
                    print_char(value);
                    break;
                }

                case 0xFF: { // HLT (stop run programm)
                    run = false;
                    break;
                }

                case 0x00: {
                    break;
                }
                
                default: {
                    print_hex(stack[cursor]);
                    print("FATAL RPOGRAM ERROR \n");
                    run = false;
                    break;
                }
            }
        }
    }
    if (strcmp(command, "ls") == 0) {
        sfs_list();
    }
    if (strcmp(command, "cat") == 0) {
        if (args != 0) {
            uint8_t buf[512];
            uint32_t size;
            if (sfs_read(args, buf, &size) == 0) {
                buf[size] = '\0';
                print((char*)buf);
                print("\n");
            }
        } else {
            print("Usage: cat <filename>\n");
        }
    }
    if (strcmp(command, "write") == 0) {
        if (args == 0) {
            print("Usage: write <file> <text>\n");
            return;
        }

        char* filename = args;
        char* text = 0;

        int i = 0;
        while (args[i] != '\0') {
            if (args[i] == ' ') {
                args[i] = '\0';

                text = &args[i + 1];
                break;
            }
            i++;
        }

        if (filename != 0 && text != 0) {
            uint32_t len = 0;
            while (text[len] != '\0') {
                len++;
            }

            if (sfs_create(filename, (uint8_t*)text, len) == 0) {
                print("Succes writen to ");
                print(filename);
                print("\n");
            } else {
                print("Error: Could not write file\n");
            }
        } else {
            print("Usage: write <file> <text>\n");
        }
    }
    if (strcmp(command, "rm") == 0) {
        if (args == 0) {
            print("Usage: del <file>\n");
            return;
        }

        if (sfs_delete(args) == 0) {
            print("Succes delete file");
            print(args);
            print("\n");
        } else {
            print("Unable to delete file\n");
        }
    }
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

void parse_command(char* cmd, char** command, char** args) {
    *command = cmd;
    *args = 0;

    int i = 0;
    while (cmd[i] != '\0') {
        if (cmd[i] == ' ') {
            cmd[i] = '\0';
            *args = &cmd[i + 1];
            break;
        }
        i++;
    }
}

void print_hex(uint8_t value) {
    char hex[] = "0123456789ABCDEF";
    print_char(hex[(value >> 4) & 0x0F]);
    print_char(hex[value & 0x0F]);
}
