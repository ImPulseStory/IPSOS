#ifndef IPSOS_GFX_H
#define IPSOS_GFX_H

#include <stdint.h>

#define GFX_IMAGE_COLORKEY 0x00000001u

#define GFX_RGB(r, g, b) \
    ((((uint32_t)(r) & 0xFFu) << 16) | (((uint32_t)(g) & 0xFFu) << 8) | ((uint32_t)(b) & 0xFFu))

typedef struct GfxImage {
    uint32_t width;
    uint32_t height;
    const uint32_t *pixels;
    uint32_t flags;
    uint32_t transparent_color;
} GfxImage;

int gfx_init(void);
int gfx_ready(void);
int gfx_present(void);
int gfx_cursor_hardware_ready(void);
int gfx_cursor_move(int x, int y);
uint32_t gfx_driver_error(void);
uint32_t gfx_width(void);
uint32_t gfx_height(void);
void gfx_clear(uint32_t color);
void gfx_draw_pixel(int x, int y, uint32_t color);
uint32_t gfx_get_pixel(int x, int y);
void gfx_draw_hline(int x, int y, int width, uint32_t color);
void gfx_draw_vline(int x, int y, int height, uint32_t color);
void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_draw_rect(int x, int y, int width, int height, uint32_t color);
void gfx_draw_square(int x, int y, int size, uint32_t color);
void gfx_draw_rect_outline(int x, int y, int width, int height, uint32_t color);
void gfx_draw_circle(int cx, int cy, int radius, uint32_t color);
void gfx_fill_circle(int cx, int cy, int radius, uint32_t color);
void gfx_draw_char(int x, int y, char character, uint32_t color);
void gfx_draw_char_bg(int x, int y, char character, uint32_t foreground, uint32_t background);
void gfx_draw_text(int x, int y, const char *text, uint32_t color);
int gfx_draw_image(int x, int y, const GfxImage *image);
void gfx_blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y);
void gfx_set_clip(int x, int y, int width, int height);
void gfx_reset_clip(void);
void gfx_get_clip(int *x, int *y, int *width, int *height);

void gfx_clear_screen(uint32_t color);
void gfx_print_char(int x, int y, char character, uint32_t color);
void gfx_print_gr_cl(const char *text, uint32_t color);
void gfx_print_char_gr_cl(char character, uint32_t color);

#endif
