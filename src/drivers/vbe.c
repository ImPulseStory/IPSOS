#include "../include/vbe.h"
#include <stdint.h>

static volatile uint32_t* framebuffer;
static uint16_t screen_width;
static uint16_t screen_height;
static uint16_t pitch;
static volatile uint32_t back_buffer[1024 * 768];

static int console_x = 0;
static int console_y = 0;

void vbe_init() {
    framebuffer = (uint32_t*)(*(uint32_t*)0x9000);
    pitch       = *(uint16_t*)0x9004;
    screen_width   = *(uint16_t*)0x9006;
    screen_height  = *(uint16_t*)0x9008;
}

void put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height) return;
    framebuffer[y * screen_width + x] = color;
}

void clear_screen(uint32_t color) {
    uint32_t w = (screen_width > 0) ? screen_width : 640;
    uint32_t h = (screen_height > 0) ? screen_height : 480;

    for (uint32_t i = 0; i < w * h; i++) {
        framebuffer[i] = color;
    }
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            put_pixel(x + j, y + i, color);
        }
    }
}

void draw_char(int x, int y, char c, uint32_t color) {
    extern const uint8_t font8x16[256][16];
    const uint8_t* glyph = font8x16[(uint8_t)c];
    
    for (int row = 0; row < 16; row++) {
        uint8_t byte = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (byte & (1 << (7 - col))) {
                put_pixel(x + col, y + row, color);
            }
        }
    }
}

void flush_screen() {
    for (int y = 0; y < 768; y++) {
        for (int x = 0; x < 1024; x++) {
            framebuffer[y * 1024 + x] = back_buffer[y * 1024 + x];
        }
    }
}

void print_char_gr(char c) {
    if (c == 'n') {
        console_x = 0;
        console_y += 16;
        if (console_y >= screen_height) {
            console_y = screen_height - 16;
        }
        return;
    }

    draw_char(console_x, console_y, c, 0xFFFFFF);
    console_x += 8;

    if (console_x >= screen_width) {
        console_x = 0;
        console_y += 16;
    }

    if (c == '\b') {
        draw_rect(console_x, console_y, 8, 16, 0x0000FF);
        console_x -= 8;
    }
}

void print_gr(const char* c) {
    while(*c) {
        print_char_gr(*c);
        c++;
    }
}