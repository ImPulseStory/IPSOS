#include "api.h"
#include <stdint.h>

void gfx_draw_pixel(int x, int y, uint32_t color) {
    put_pixel(x, y, color);
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    draw_rect(x, y, w, h, color);
}

void gfx_clear_screen(uint32_t color) {
    clear_screen(color);
}

void gfx_print_char(int x, int y, char c, uint32_t color) {
    draw_char(x, y, c, color);
}

uint32_t gfx_get_pixel(int x, int y) {
    return get_pixel(x, y);
}

void gfx_print_gr_cl(const char* c, uint32_t color) {
    print_gr_cl(c, color);
}

void gfx_print_char_gr_cl(char c, uint32_t color) {
    print_char_gr_cl(c, color);
}