#include "edit.h"
#include <stdint.h>
#include <string.h>

#define EDIT_BUFFER_SIZE 2048

static uint32_t line_start(const char *buf, uint32_t pos) {
    while (pos > 0 && buf[pos - 1] != '\n') {
        pos--;
    }

    return pos;
}

static uint32_t line_end(const char *buf, uint32_t pos, uint32_t len) {
    while (pos < len && buf[pos] != '\n') {
        pos++;
    }

    return pos;
}

static uint32_t get_column(const char *buf, uint32_t pos) {
    return pos - line_start(buf, pos);
}

static uint32_t move_up(const char *buf, uint32_t cursor) {
    uint32_t column = get_column(buf, cursor);
    uint32_t start = line_start(buf, cursor);

    if (start == 0) {
        return cursor;
    }

    uint32_t prev_end = start - 1;
    uint32_t prev_start = line_start(buf, prev_end);
    uint32_t target = prev_start + column;

    if (target > prev_end) {
        target = prev_end;
    }

    return target;
}

static uint32_t move_down(const char *buf, uint32_t cursor, uint32_t len) {
    uint32_t column = get_column(buf, cursor);
    uint32_t end = line_end(buf, cursor, len);

    if (end >= len) {
        return cursor;
    }

    uint32_t next_start = end + 1;
    uint32_t next_end = line_end(buf, next_start, len);
    uint32_t target = next_start + column;

    if (target > next_end) {
        target = next_end;
    }

    return target;
}

static void redraw(const char *buf, uint32_t len, uint32_t cursor) {
    term_clear();

    for (uint32_t i = 0; i <= len; i++) {
        if (i == cursor) {
            term_print_char('|');
        }

        if (i < len) {
            term_print_char(buf[i]);
        }
    }
}

static void insert_char(
    char *buf,
    uint32_t *len,
    uint32_t *cursor,
    char ch
) {
    if (*len >= EDIT_BUFFER_SIZE - 1) {
        return;
    }

    for (uint32_t i = *len; i > *cursor; i--) {
        buf[i] = buf[i - 1];
    }

    buf[*cursor] = ch;

    (*cursor)++;
    (*len)++;

    buf[*len] = '\0';
}

static void delete_before_cursor(
    char *buf,
    uint32_t *len,
    uint32_t *cursor
) {
    if (*cursor == 0) {
        return;
    }

    for (uint32_t i = *cursor - 1; i < *len - 1; i++) {
        buf[i] = buf[i + 1];
    }

    (*cursor)--;
    (*len)--;

    buf[*len] = '\0';
}

void run(const char *file) {
    char safe_filename[32];
    uint32_t filename_len = 0;

    while (file[filename_len] != '\0' && filename_len < sizeof(safe_filename) - 1) {
        safe_filename[filename_len] = file[filename_len];
        filename_len++;
    }

    safe_filename[filename_len] = '\0';

    char buf[EDIT_BUFFER_SIZE];
    uint32_t len = 0;
    uint32_t cursor = 0;

    memset(buf, 0, sizeof(buf));

    ips_read_file(
        safe_filename,
        (uint8_t *)buf,
        EDIT_BUFFER_SIZE - 1,
        &len
    );

    if (len >= EDIT_BUFFER_SIZE) {
        len = EDIT_BUFFER_SIZE - 1;
    }

    buf[len] = '\0';

    redraw(buf, len, cursor);

    while (1) {
        int key = get_key();

        if (key == 0) {
            continue;
        }

        if (key == KEY_ESC) {
            ips_write_file(safe_filename, buf, len);
            term_clear();
            return;
        }

        if (key == KEY_LEFT) {
            if (cursor > 0) {
                cursor--;
            }
        }
        else if (key == KEY_RIGHT) {
            if (cursor < len) {
                cursor++;
            }
        }
        else if (key == KEY_UP) {
            cursor = move_up(buf, cursor);
        }
        else if (key == KEY_DOWN) {
            cursor = move_down(buf, cursor, len);
        }
        else if (key == KEY_BACKSPACE) {
            delete_before_cursor(buf, &len, &cursor);
        }
        else if (key == KEY_ENTER) {
            insert_char(buf, &len, &cursor, '\n');
        }
        else if (key == KEY_TAB) {
            for (int i = 0; i < 4; i++) {
                insert_char(buf, &len, &cursor, ' ');
            }
        }
        else if (key >= 32 && key <= 126) {
            insert_char(buf, &len, &cursor, (char)key);
        }

        redraw(buf, len, cursor);
    }
}