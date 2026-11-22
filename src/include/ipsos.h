#ifndef IPSOS_API_H
#define IPSOS_API_H
#include <stdint.h>
#include <stddef.h>
#include "task.h"
void term_print(const char *str);
void term_print_char(char c);
void term_print_int(int value);
void term_clear(void);
void gfx_draw_pixel(int x, int y, uint32_t color);
void gfx_draw_rect(int x, int y, int width, int height, uint32_t color);
void gfx_clear_screen(uint32_t color);
void gfx_print_char(int x, int y, char c, uint32_t color);
uint32_t gfx_get_pixel(int x, int y);
int create_file(const char *name);
int deleteEx(const char *name);
int read_file(const char *name, char *buffer, uint32_t capacity, uint32_t *out_size);
int write_to_file(const char *name, const char *data);
int list(char *buffer, const char *path);
int list_ex(char *buffer, const char *path, uint32_t capacity);
int mkdir(const char *path);
#endif
