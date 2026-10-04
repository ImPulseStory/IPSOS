#include "boot.h"

static const char scancode_to_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static char buffer[256];
static int  buffer_index = 0;
volatile int input_reade = 0;

void keyboard_init() {

}

void keyboard_handler() {


    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode >= 0x80) {
        return;
    }

    char ascii = scancode_to_ascii[scancode];

    if (ascii == 0) {
        return;
    }

    if (ascii == '\n') {
        buffer[buffer_index] = '\0';
        buffer_index = 0;
        input_reade = 1;
        print_char('\n');
        return;
    }

    if (ascii == '\b') {
        if (buffer_index > 0) {
            buffer_index--;
            print_char('\b');
        }
    }

    if (buffer_index < 255) {
        buffer[buffer_index] = ascii;
        buffer_index++;
        print_char(ascii);
    }
}

char* input() {
    input_reade = 0;
    buffer_index = 0;
    buffer[0] = '\0';
    
    while (input_reade == 0) {
        __asm__ volatile ("hlt");
    }
    return buffer;
}