#ifndef KEYBOARD_H 
#define KEYBOARD_H

#define KEYBOARD_DATA_PORT 0x60
#define PIC1_CMD           0x20

void keyboard_init();
void keyboard_handler();
char* input();

#endif