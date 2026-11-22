
#include "../include/keyboard.h"
#include "../include/io.h"
#include "../apps/terminal/terminal.h"
#include <stdint.h>

static const char scancode_lower[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char scancode_upper[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static volatile int key_queue[256];
static volatile int q_head = 0;
static volatile int q_tail = 0;

static int is_extended = 0;
static int shift_pressed = 0;
static int caps_lock = 0;

static void keyboard_queue_key(int key) {
    int next_head = (q_head + 1) % 256;

    if (next_head != q_tail) {
        key_queue[q_head] = key;
        q_head = next_head;
    }
}

void keyboard_init(void) {
    q_head = 0;
    q_tail = 0;
    is_extended = 0;
    shift_pressed = 0;
    caps_lock = 0;
}

void keyboard_handler(void) {
    uint8_t status = inb(0x64);

    if (!(status & 0x01) || (status & 0x20)) {
        return;
    }

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode == 0xE0) {
        is_extended = 1;
        return;
    }

    int extended = is_extended;
    is_extended = 0;

    int released = scancode & 0x80;
    uint8_t code = scancode & 0x7F;

    if (released) {
        if (!extended && (code == 0x2A || code == 0x36)) {
            shift_pressed = 0;
        }
        return;
    }

    int key = 0;

    if (extended) {
        switch (code) {
            case 0x48: key = KEY_UP; break;
            case 0x50: key = KEY_DOWN; break;
            case 0x4B: key = KEY_LEFT; break;
            case 0x4D: key = KEY_RIGHT; break;
            case 0x53: key = KEY_DELETE; break;
            default: break;
        }
    } else {
        if (code == 0x01) {
            key = KEY_ESC;
        } else if (code == 0x1C) {
            key = '\n';
        } else if (code == 0x2A || code == 0x36) {
            shift_pressed = 1;
            return;
        } else if (code == 0x3A) {
            caps_lock = !caps_lock;
            return;
        } else if (code < 128) {
            char ascii;

            if ((code >= 0x10 && code <= 0x19) ||
                (code >= 0x1E && code <= 0x26) ||
                (code >= 0x2C && code <= 0x32)) {
                ascii = (shift_pressed ^ caps_lock)
                    ? scancode_upper[code]
                    : scancode_lower[code];
            } else {
                ascii = shift_pressed
                    ? scancode_upper[code]
                    : scancode_lower[code];
            }

            if (ascii != 0) {
                key = ascii;
            }
        }
    }

    if (key != 0) {
        keyboard_queue_key(key);
    }
}

char *input(void) {
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
        } else if (key == '\b') {
            if (index > 0) {
                index--;
                term_print_char('\b');
            }
        } else if (key >= 32 && key <= 126) {
            if (index < 255) {
                buffer[index++] = (char)key;
                term_print_char((char)key);
            }
        }
    }
}

int get_key(void) {
    if (q_head == q_tail) {
        return 0;
    }

    int key = key_queue[q_tail];
    q_tail = (q_tail + 1) % 256;
    return key;
}
