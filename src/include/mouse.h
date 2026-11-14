#ifndef MOUSE_H
#define MOUSE_H

#include "io.h"

#define DATA_PORT    0x60
#define COMMAND_PORT 0x64

#define MOUSE_ON     0xA8
#define CONF_BYTE    0x20

void mouse_init();
void mouse_handler();

int get_mouse_x();
int get_mouse_y();
int get_mouse_moved();

#endif