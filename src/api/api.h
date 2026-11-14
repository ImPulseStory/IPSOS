#ifndef API_H
#define API_H

#include "../include/vbe.h"
#include "../include/keyboard.h"
#include "../include/ipsfs.h"
#include "../include/mouse.h"
#include <stdint.h>

// # ----------- VBE ------------ # //
void gfx_draw_pixel(int x, int y, uint32_t color);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color);
void gfx_clear_screen(uint32_t color);
void gfx_print_char(int x, int y, char c, uint32_t color);
uint32_t gfx_get_pixel(int x, int y);

// # ----------- KEYBOARD -------- # //
char* input();
char readKey();

// # ----------- MOUSE ------------ # //
int mouse_x();
int mouse_y();
int mouse_moved();

// # ------------ FILES ----------- # //
int create_file(const char* name);
int deleteEx(const char* name);
int read_file(const char* name, char* buf);
int write_to_file(const char* name, const char* data);
int list(char* buf, const char* path);
int mkdir(const char* path);

#endif