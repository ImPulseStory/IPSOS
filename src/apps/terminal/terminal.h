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

#endif