#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define WHITE_ON_BLACK 0x0F
#define VGA_ADDRESS (volatile unsigned short *) 0xB8000

void print_hex(uint8_t value);
void scroll();
void print(const char *c);
void print_char(char c);
void clear();
void printf(const char* fmt, ...);
void parse_command(char* cmd, char** command, char** args);

#endif