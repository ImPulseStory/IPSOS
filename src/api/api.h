#ifndef API_H
#define API_H

#include "../include/gfx.h"
#include "../include/virgl.h"
#include "../include/gl.h"
#include "../include/keyboard.h"
#include "../include/ipsfs.h"
#include "../include/mouse.h"
#include <stdint.h>

char *input(void);
char readKey(void);

int mouse_x(void);
int mouse_y(void);
int mouse_moved(void);
int mouse_buttons(void);

int create_file(const char *name);
int deleteEx(const char *name);
int read_file(const char *name, char *buf, uint32_t capacity, uint32_t *out_size);
int write_to_file(const char *name, const char *data);
int list(char *buffer, const char *path);
int list_ex(char *buffer, const char *path, uint32_t capacity);
int mkdir(const char *path);

void gfx_print_char(int x, int y, char character, uint32_t color);
void gfx_print_gr_cl(const char *text, uint32_t color);
void gfx_print_char_gr_cl(char character, uint32_t color);

#endif
