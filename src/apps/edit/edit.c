#include "edit.h"
#include <stdint.h>
#include <string.h>
#include "../../include/desktop.h"
#include "../../include/gfx.h"
#include "../../include/keyboard.h"
#include "../../include/ipsfs.h"

#define EDIT_BUFFER_SIZE 8192
#define EDIT_PATH_SIZE  256
#define EDIT_LINE_HEIGHT 16
#define EDIT_GUTTER      48
#define EDIT_TOPBAR      24
#define EDIT_STATUSBAR   20

#define C_BG       GFX_RGB(20, 23, 30)
#define C_TOP      GFX_RGB(29, 34, 44)
#define C_STATUS   GFX_RGB(35, 42, 54)
#define C_TEXT     GFX_RGB(210, 216, 226)
#define C_MUTED    GFX_RGB(105, 117, 135)
#define C_GUTTER   GFX_RGB(26, 30, 39)
#define C_LINE     GFX_RGB(31, 37, 48)
#define C_CURSOR   GFX_RGB(245, 190, 90)
#define C_KEYWORD  GFX_RGB(198, 120, 221)
#define C_STRING  GFX_RGB(152, 195, 121)
#define C_COMMENT GFX_RGB(106, 130, 112)
#define C_NUMBER  GFX_RGB(209, 154, 102)
#define C_TYPE    GFX_RGB(86, 182, 194)

static char editor_buf[EDIT_BUFFER_SIZE];
static char editor_path[EDIT_PATH_SIZE];
static uint32_t editor_len, editor_cursor, editor_scroll;
static int editor_dirty, editor_overwrite, editor_confirm_close, editor_help;
static int editor_saved_ok;
static Window_t *editor_window;

static uint32_t line_start(const char *s, uint32_t p) {
    while (p && s[p - 1] != '\n') --p;
    return p;
}
static uint32_t line_end(const char *s, uint32_t p, uint32_t n) {
    while (p < n && s[p] != '\n') ++p;
    return p;
}
static uint32_t line_number_at(uint32_t p) {
    uint32_t line = 1;
    for (uint32_t i = 0; i < p && i < editor_len; ++i)
        if (editor_buf[i] == '\n') ++line;
    return line;
}
static uint32_t line_start_by_number(uint32_t wanted) {
    uint32_t line = 1;
    for (uint32_t i = 0; i < editor_len; ++i) {
        if (line == wanted) return i;
        if (editor_buf[i] == '\n') ++line;
    }
    return editor_len;
}
static uint32_t column_at(uint32_t p) {
    uint32_t start = line_start(editor_buf, p), col = 0;
    for (uint32_t i = start; i < p; ++i)
        col += editor_buf[i] == '\t' ? 4u - (col & 3u) : 1u;
    return col;
}
static uint32_t move_vertical(uint32_t p, int direction) {
    uint32_t col = column_at(p), start = line_start(editor_buf, p);
    uint32_t target_start, target_end, target;
    if (direction < 0) {
        if (!start) return p;
        target_end = start - 1;
        target_start = line_start(editor_buf, target_end);
    } else {
        uint32_t end = line_end(editor_buf, p, editor_len);
        if (end >= editor_len) return p;
        target_start = end + 1;
        target_end = line_end(editor_buf, target_start, editor_len);
    }
    target = target_start;
    while (target < target_end && col) {
        col -= editor_buf[target] == '\t' ? 4u - (col & 3u) : 1u;
        if (col == 0) break;
        ++target;
    }
    if (target > target_end) target = target_end;
    return target;
}
static int is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}
static int word_is(const char *s, uint32_t n, const char *word) {
    uint32_t i = 0;
    while (word[i] && i < n && s[i] == word[i]) ++i;
    return word[i] == 0 && i == n;
}
static uint32_t word_color(const char *s, uint32_t n) {
    static const char *keywords[] = {
        "if","else","for","while","do","switch","case","default",
        "break","continue","return","goto","sizeof","typedef",
        "static","extern","const","volatile","inline","struct",
        "union","enum","void","char","short","int","long",
        "float","double","signed","unsigned","auto","register",
        "NULL","true","false"
    };
    static const char *types[] = {
        "uint8_t","uint16_t","uint32_t","uint64_t","int8_t",
        "int16_t","int32_t","int64_t","size_t","uintptr_t"
    };
    for (uint32_t i = 0; i < sizeof(keywords)/sizeof(keywords[0]); ++i)
        if (word_is(s, n, keywords[i])) return C_KEYWORD;
    for (uint32_t i = 0; i < sizeof(types)/sizeof(types[0]); ++i)
        if (word_is(s, n, types[i])) return C_TYPE;
    return C_TEXT;
}
static void draw_line_text(int x, int y, uint32_t start, uint32_t end) {
    uint32_t i = start;
    int col = 0, in_string = 0, in_char = 0;
    while (i < end) {
        uint32_t color = C_TEXT;
        char c = editor_buf[i];
        if (c == '/' && i + 1 < end && editor_buf[i + 1] == '/') {
            while (i < end) {
                gfx_draw_char(x + col * 8, y, editor_buf[i++], C_COMMENT);
                ++col;
            }
            return;
        }
        if (c == '"' && !in_char) { in_string = !in_string; color = C_STRING; }
        else if (c == '\'' && !in_string) { in_char = !in_char; color = C_STRING; }
        else if (in_string || in_char) color = C_STRING;
        else if (c >= '0' && c <= '9') color = C_NUMBER;
        else if (is_word_char(c) && !(c >= '0' && c <= '9')) {
            uint32_t j = i;
            while (j < end && is_word_char(editor_buf[j])) ++j;
            color = word_color(editor_buf + i, j - i);
            for (; i < j; ++i) {
                gfx_draw_char(x + col * 8, y, editor_buf[i], color);
                ++col;
            }
            continue;
        }
        if (c == '\t') {
            int spaces = 4 - (col & 3);
            for (int k = 0; k < spaces; ++k) {
                gfx_draw_char(x + col * 8, y, ' ', C_TEXT);
                ++col;
            }
        } else {
            gfx_draw_char(x + col * 8, y, c, color);
            ++col;
        }
        if ((in_string || in_char) && c == '\\' && i + 1 < end) {
            ++i;
            gfx_draw_char(x + col * 8, y, editor_buf[i], C_STRING);
            ++col;
        }
        ++i;
    }
}
static void editor_render(Window_t *window) {
    int x = window->x + 1, y = window->y + 22;
    int w = window->w - 2, h = window->h - 23;
    int text_y, visible_lines;
    uint32_t pos = 0, cursor_line, cursor_col;
    if (w <= 0 || h <= 0) return;

    gfx_draw_rect(x, y, w, h, C_BG);
    gfx_draw_rect(x, y, w, EDIT_TOPBAR, C_TOP);
    gfx_draw_text(x + 8, y + 4, "F2 Save   ESC Close   F1 Help", C_MUTED);
    if (editor_dirty) gfx_draw_text(x + w - 72, y + 4, "Modified", C_CURSOR);
    else gfx_draw_text(x + w - 56, y + 4, "Saved", C_STRING);

    text_y = y + EDIT_TOPBAR;
    visible_lines = (h - EDIT_TOPBAR - EDIT_STATUSBAR) / EDIT_LINE_HEIGHT;
    if (visible_lines < 1) visible_lines = 1;
    pos = line_start_by_number(editor_scroll + 1);
    cursor_line = line_number_at(editor_cursor);
    cursor_col = column_at(editor_cursor);

    for (int row = 0; row < visible_lines && pos <= editor_len; ++row) {
        uint32_t end = line_end(editor_buf, pos, editor_len);
        int row_y = text_y + row * EDIT_LINE_HEIGHT;
        int line_no = (int)(editor_scroll + row + 1);
        if (line_no == (int)cursor_line)
            gfx_draw_rect(x, row_y, w, EDIT_LINE_HEIGHT, C_LINE);
        gfx_draw_rect(x, row_y, EDIT_GUTTER - 4, EDIT_LINE_HEIGHT, C_GUTTER);
        char number[12];
        int digits = 0, v = line_no;
        if (v <= 0) v = 1;
        do { number[digits++] = (char)('0' + v % 10); v /= 10; } while (v && digits < 10);
        for (int d = 0; d < digits; ++d)
            gfx_draw_char(x + EDIT_GUTTER - 12 - d * 8, row_y, number[digits - 1 - d], C_MUTED);
        draw_line_text(x + EDIT_GUTTER, row_y, pos, end);

        if (line_no == (int)cursor_line) {
            int cx = x + EDIT_GUTTER + (int)cursor_col * 8;
            if (cx < x + w - 2) {
                if (editor_overwrite && editor_cursor < editor_len && editor_buf[editor_cursor] != '\n')
                    gfx_draw_rect_outline(cx, row_y + 1, 8, EDIT_LINE_HEIGHT - 2, C_CURSOR);
                else
                    gfx_draw_vline(cx, row_y + 1, EDIT_LINE_HEIGHT - 2, C_CURSOR);
            }
        }
        if (end >= editor_len) break;
        pos = end + 1;
    }

    gfx_draw_rect(x, y + h - EDIT_STATUSBAR, w, EDIT_STATUSBAR, C_STATUS);
    char status[96];
    status[0] = '\0';
    strcpy(status, "Ln ");
    uint32_t ln = cursor_line, digits = 0;
    char rev[12];
    do { rev[digits++] = (char)('0' + ln % 10); ln /= 10; } while (ln && digits < 10);
    for (uint32_t d = 0; d < digits; ++d) {
        uint32_t n = strlen(status); status[n] = rev[digits - 1 - d]; status[n + 1] = 0;
    }
    strcat(status, "  Col ");
    uint32_t cn = cursor_col + 1, cd = 0;
    do { rev[cd++] = (char)('0' + cn % 10); cn /= 10; } while (cn && cd < 10);
    for (uint32_t d = 0; d < cd; ++d) {
        uint32_t n = strlen(status); status[n] = rev[cd - 1 - d]; status[n + 1] = 0;
    }
    strcat(status, editor_overwrite ? "  OVR" : "  INS");
    gfx_draw_text(x + 8, y + h - EDIT_STATUSBAR + 2, status, C_TEXT);
    if (editor_confirm_close)
        gfx_draw_text(x + 8, y + h - EDIT_STATUSBAR + 2, "Unsaved changes: F2 save, ESC discard", C_CURSOR);
    else if (!editor_saved_ok)
        gfx_draw_text(x + 8, y + h - EDIT_STATUSBAR + 2, "Save failed - check file path / disk space", C_CURSOR);
    else if (editor_help)
        gfx_draw_text(x + 8, y + h - EDIT_STATUSBAR + 2, "Arrows/Home/End/PgUp/PgDn  Del  Tab  F2 save  F1 hide", C_CURSOR);
}
static void editor_invalidate(void) {
    if (editor_window)
        desktop_invalidate_rect(editor_window->x + 1, editor_window->y + 22,
                             editor_window->w - 2, editor_window->h - 23);
}
static void editor_save(void) {
    int result = ips_write_file(editor_path, editor_buf, editor_len);
    if (result == (int)editor_len) {
        editor_dirty = 0;
        editor_saved_ok = 1;
        editor_confirm_close = 0;
    } else {
        editor_saved_ok = 0;
    }
}
static void editor_insert(char c) {
    if (editor_overwrite && editor_cursor < editor_len && editor_buf[editor_cursor] != '\n') {
        editor_buf[editor_cursor++] = c;
        editor_dirty = 1;
        return;
    }
    if (editor_len >= EDIT_BUFFER_SIZE - 1) return;
    for (uint32_t i = editor_len; i > editor_cursor; --i)
        editor_buf[i] = editor_buf[i - 1];
    editor_buf[editor_cursor++] = c;
    ++editor_len;
    editor_buf[editor_len] = 0;
    editor_dirty = 1;
}
static void editor_delete_forward(void) {
    if (editor_cursor >= editor_len) return;
    for (uint32_t i = editor_cursor; i + 1 < editor_len; ++i)
        editor_buf[i] = editor_buf[i + 1];
    --editor_len;
    editor_buf[editor_len] = 0;
    editor_dirty = 1;
}
static void editor_ensure_cursor_visible(void) {
    uint32_t ln = line_number_at(editor_cursor);
    int h = editor_window ? editor_window->h - 23 : 600;
    uint32_t visible = (uint32_t)((h - EDIT_TOPBAR - EDIT_STATUSBAR) / EDIT_LINE_HEIGHT);
    if (!visible) visible = 1;
    if (ln <= editor_scroll) editor_scroll = ln - 1;
    else if (ln > editor_scroll + visible) editor_scroll = ln - visible;
}

void run(const char *file) {
    uint32_t path_len = 0, loaded = 0;
    if (!file) return;
    while (file[path_len] && path_len < EDIT_PATH_SIZE - 1) {
        editor_path[path_len] = file[path_len];
        ++path_len;
    }
    editor_path[path_len] = 0;
    memset(editor_buf, 0, sizeof(editor_buf));
    editor_len = editor_cursor = editor_scroll = 0;
    editor_dirty = editor_overwrite = editor_confirm_close = editor_help = 0;
    editor_saved_ok = 1;
    ips_read_file(editor_path, editor_buf, EDIT_BUFFER_SIZE - 1, &loaded);
    if (loaded >= EDIT_BUFFER_SIZE) loaded = EDIT_BUFFER_SIZE - 1;
    editor_len = loaded;
    editor_buf[editor_len] = 0;

    int sw = (int)gfx_width(), sh = (int)gfx_height();
    int ww = sw > 1000 ? 900 : sw - 40;
    int wh = sh > 700 ? 620 : sh - 60;
    if (ww < 320) ww = sw;
    if (wh < 240) wh = sh;
    editor_window = wm_create((sw - ww) / 2, (sh - wh) / 2, ww, wh, editor_path, 1);
    if (!editor_window) {
        term_print("edit: unable to create editor window\n");
        return;
    }
    wm_set_renderer(editor_window, editor_render);
    editor_invalidate();

    while (editor_window && !editor_window->closed) {
        int key = get_key();
        if (!key) continue;
        if (key == KEY_F2) {
            editor_save();
        } else if (key == KEY_F1) {
            editor_help = !editor_help;
        } else if (key == KEY_ESC) {
            if (editor_dirty && !editor_confirm_close) {
                editor_confirm_close = 1;
            } else {
                wm_destroy(editor_window);
                editor_window = 0;
                break;
            }
        } else if (key == KEY_LEFT) {
            if (editor_cursor) --editor_cursor;
        } else if (key == KEY_RIGHT) {
            if (editor_cursor < editor_len) ++editor_cursor;
        } else if (key == KEY_UP) {
            editor_cursor = move_vertical(editor_cursor, -1);
        } else if (key == KEY_DOWN) {
            editor_cursor = move_vertical(editor_cursor, 1);
        } else if (key == KEY_HOME) {
            editor_cursor = line_start(editor_buf, editor_cursor);
        } else if (key == KEY_END) {
            editor_cursor = line_end(editor_buf, editor_cursor, editor_len);
        } else if (key == KEY_PAGE_UP || key == KEY_PAGE_DOWN) {
            int h = editor_window->h - 23;
            int rows = (h - EDIT_TOPBAR - EDIT_STATUSBAR) / EDIT_LINE_HEIGHT;
            if (rows < 1) rows = 1;
            for (int i = 0; i < rows; ++i)
                editor_cursor = move_vertical(editor_cursor, key == KEY_PAGE_UP ? -1 : 1);
        } else if (key == KEY_BACKSPACE) {
            if (editor_cursor) {
                --editor_cursor;
                editor_delete_forward();
            }
        } else if (key == KEY_DELETE) {
            editor_delete_forward();
        } else if (key == KEY_INSERT) {
            editor_overwrite = !editor_overwrite;
        } else if (key == KEY_ENTER) {
            editor_insert('\n');
        } else if (key == KEY_TAB) {
            for (int i = 0; i < 4; ++i) editor_insert(' ');
        } else if (key >= 32 && key <= 126) {
            editor_insert((char)key);
        }
        editor_ensure_cursor_visible();
        editor_invalidate();
    }
    if (editor_window) {
        /* The window-manager close button cannot ask for confirmation here;
           save on that path so a mouse close never silently loses edits. */
        if (editor_window->closed && editor_dirty)
            editor_save();
        wm_destroy(editor_window);
        editor_window = 0;
    }
}
