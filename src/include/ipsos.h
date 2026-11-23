#ifndef IPSOS_API_H
#define IPSOS_API_H

#include <stdint.h>
#include <stddef.h>
#include "task.h"
#include "gfx.h"

void term_print(const char *str);
void term_print_char(char c);
void term_print_int(int value);
void term_clear(void);
char *input(void);
char readKey(void);
int mouse_x(void);
int mouse_y(void);
int mouse_moved(void);
int mouse_buttons(void);
int create_file(const char *name);
int deleteEx(const char *name);
int read_file(const char *name, char *buffer, uint32_t capacity, uint32_t *out_size);
int write_to_file(const char *name, const char *data);
int list(char *buffer, const char *path);
int list_ex(char *buffer, const char *path, uint32_t capacity);
int mkdir(const char *path);

#endif
