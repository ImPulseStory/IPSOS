#ifndef VBE_H
#define VBE_H

#include "font.h"

#include <stdint.h>

void vbe_init();
void put_pixel(int x, int y, uint32_t color);
void draw_rect(int x, int y, int w, int h, uint32_t color);
void clear_screen(uint32_t color);
void flush_screen();
void draw_char(int x, int y, char c, uint32_t color);
void print_char_gr(char c);
void print_gr(const char* c);

#endif