#include "../include/keyboard.h"
#include "../include/io.h"
#include "../include/vga.h"
#include "../apps/terminal/terminal.h"
#include <stdint.h>

static const char scancode_to_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int key_queue[256];
static int q_head = 0;
static int q_tail = 0;

static int is_extended = 0;

void keyboard_init() {
}

void keyboard_handler() {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode & 0x80) return;

    int key = 0;

    if (scancode == 0xE0) {
        is_extended = 1;
        return;
    }

    if (is_extended) {
        is_extended = 0;
        if (scancode == 0x48) key = KEY_UP;
        else if (scancode == 0x50) key = KEY_DOWN;
        else if (scancode == 0x4B) key = KEY_LEFT;
        else if (scancode == 0x4D) key = KEY_RIGHT;
        else if (scancode == 0x53) key = KEY_DELETE;
    } else {
        if (scancode == 0x01) key = KEY_ESC;
        else if (scancode == 0x0E) key = '\b';
        else if (scancode == 0x0F) key = '\t';
        else if (scancode == 0x1C) key = '\n';
        else if (scancode < 128) {
            char ascii = scancode_to_ascii[scancode];
            if (ascii != 0) key = ascii;
        }
    }

    if (key != 0) {
        int next_head = (q_head + 1) % 256;
        if (next_head != q_tail) {
            key_queue[q_head] = key;
            q_head = next_head;
        }
    }
}

char* input() {
    static char buffer[256];
    int index = 0;
    buffer[0] = '\0';

    while (1) {
        int key = get_key();
        if (key == 0) {
            __asm__ volatile ("hlt");
            continue;
        }

        if (key == '\n') {
            buffer[index] = '\0';
            term_print_char('\n');
            return buffer;
        } 
        else if (key == '\b') {
            if (index > 0) {
                index--;
                term_print_char('\b');
            }
        } 
        else if (key >= 32 && key <= 126) {
            if (index < 255) {
                buffer[index] = (char)key;
                index++;
                term_print_char((char)key);
            }
        }
    }
}

int get_key() {
    if (q_head == q_tail) return 0;
    int key = key_queue[q_tail];
    q_tail = (q_tail + 1) % 256;
    return key;
}