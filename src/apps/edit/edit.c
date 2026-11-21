#include "edit.h"
#include <stdint.h>

void run(const char* file) {
    term_clear();

    char safe_filename[32];
    int i = 0;
    while (file[i] != '\0' && i < 31) {
        safe_filename[i] = file[i];
        i++;
    }
    safe_filename[i] = '\0';

    char buf[2048];
    uint32_t len = 0;
    
    for (int k = 0; k < 2048; k++) {
        buf[k] = 0;
    }

    uint32_t cursor = 0;

    int running = 1;
    while (running) {
        int key = get_key();
        if (key == 0) continue;

        if (key == KEY_ESC) {
            write_to_file(safe_filename, (char*)buf);
            running = 0;
            term_clear();
            break;
        }

        if (key >= 32 && key <= 126) {
            for (uint32_t j = len; j > cursor; j--) {
                buf[j] = buf[j - 1];
            }
            buf[cursor] = (char)key;
            cursor++;
            len++;
        }

        else if (key == KEY_BACKSPACE) {
            if (cursor > 0) {
                for (uint32_t j = cursor - 1; j < len - 1; j++) {
                    buf[j] = buf[j + 1];
                }
                cursor--;
                len--;
                buf[len] = 0;
                term_print_char('\b');
            }
        }

        else if (key == KEY_TAB) {
            for (int i = 0; i < 4; i++) {
                buf[cursor] = ' ';
                cursor++;
                len++;
                term_print_char(' ');
            }
        }

        else if (key == KEY_ENTER) {
            for (uint32_t j = len; j > cursor; j--) {
                buf[j] = buf[j - 1];
            }
            buf[cursor] = '\n';
            cursor++;
            len++;
            term_print_char('\n');
        }

        else if (key == KEY_LEFT) {
            if (cursor > 0) cursor--;
        }
        else if (key == KEY_RIGHT) {
            if (cursor < len) cursor++;
        }

        if (key >= 32 && key <= 126 && cursor == len) {
            term_print_char((char)key);
        }
    }
}