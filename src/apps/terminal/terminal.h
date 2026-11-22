#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stdbool.h>

#include "../../include/libc.h"
#include "../../api/api.h"

void term_print_char(char c);
void term_print(const char* str);
void term_clear();
void term_run();
void term_update();

void term_print_cl(const char* str, uint32_t color);
void term_print_char_cl(char c, uint32_t color);

void build_path(char* out, const char* arg);
void term_execute(const char *cmd);
void cmd_ipscc(const char *args);
void term_print_int(int n);
void update_terminal();

#endif