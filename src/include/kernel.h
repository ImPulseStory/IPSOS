#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include "io.h"
#include "vga.h"
#include "ata.h"
#include "idt.h"
#include "keyboard.h"
#include "pic.h"
#include "sfs.h"

void execute(const char *cmd);
void kernel_main();

#endif