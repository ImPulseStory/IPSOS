#ifndef KEYBOARD_H 
#define KEYBOARD_H

#define KEYBOARD_DATA_PORT 0x60
#define PIC1_CMD           0x20

#define KEY_ESC        27
#define KEY_BACKSPACE  '\b'
#define KEY_TAB        '\t'
#define KEY_ENTER      '\n'
#define KEY_SPACE      0x39

#define KEY_F1         0x80
#define KEY_F2         0x81
#define KEY_F3         0x82
#define KEY_F4         0x83
#define KEY_F5         0x84
#define KEY_F6         0x85
#define KEY_F7         0x86
#define KEY_F8         0x87
#define KEY_F9         0x88
#define KEY_F10        0x89
#define KEY_F11        0x8A
#define KEY_F12        0x8B

#define KEY_UP         0x90
#define KEY_DOWN       0x91
#define KEY_LEFT       0x92
#define KEY_RIGHT      0x93

#define KEY_HOME       0x94
#define KEY_END        0x95
#define KEY_PAGE_UP    0x96
#define KEY_PAGE_DOWN  0x97
#define KEY_INSERT     0x98
#define KEY_DELETE     0x99

#define KEY_LEFT_SHIFT  0xA0
#define KEY_RIGHT_SHIFT 0xA1
#define KEY_LEFT_CTRL   0xA2
#define KEY_RIGHT_CTRL  0xA3
#define KEY_LEFT_ALT    0xA4
#define KEY_RIGHT_ALT   0xA5
#define KEY_CAPS_LOCK   0xA6
#define KEY_NUM_LOCK    0xA7
#define KEY_SCROLL_LOCK 0xA8

void keyboard_init();
void keyboard_handler();
char* input();
int get_key();

#endif