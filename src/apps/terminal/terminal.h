#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stdbool.h>

#include "../../include/vbe.h"
#include "../../include/keyboard.h"
#include "../../include/libc.h"
#include "../../include/sfs.h"
#include "../../include/ata.h"

void term_print_char(char c);
void term_print(const char* str);
void term_clear();
void term_run();

#endif