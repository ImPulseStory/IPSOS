#include "api.h"
#include "../drivers/gfx_driver.h"
#include "../include/virtio_gpu.h"
#include <stdint.h>

static int console_x;
static int console_y;

int gfx_init(void)
{
    return gfxdrv_init();
}

int gfx_ready(void)
{
    return gfxdrv_ready();
}

int gfx_present(void)
{
    return gfxdrv_present();
}

int gfx_cursor_hardware_ready(void)
{
    return virtio_gpu_cursor_ready();
}

int gfx_cursor_move(int x, int y)
{
    return virtio_gpu_cursor_move(x, y);
}

uint32_t gfx_driver_error(void)
{
    return gfxdrv_error();
}

uint32_t gfx_width(void)
{
    return gfxdrv_width();
}

uint32_t gfx_height(void)
{
    return gfxdrv_height();
}

void gfx_clear(uint32_t color)
{
    gfxdrv_clear(color);
}

void gfx_draw_pixel(int x, int y, uint32_t color)
{
    gfxdrv_draw_pixel(x, y, color);
}

uint32_t gfx_get_pixel(int x, int y)
{
    return gfxdrv_get_pixel(x, y);
}

void gfx_draw_hline(int x, int y, int width, uint32_t color)
{
    gfxdrv_draw_hline(x, y, width, color);
}

void gfx_draw_vline(int x, int y, int height, uint32_t color)
{
    gfxdrv_draw_vline(x, y, height, color);
}

void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    gfxdrv_draw_line(x0, y0, x1, y1, color);
}

void gfx_draw_rect(int x, int y, int width, int height, uint32_t color)
{
    gfxdrv_draw_rect(x, y, width, height, color);
}

void gfx_draw_square(int x, int y, int size, uint32_t color)
{
    gfxdrv_draw_rect(x, y, size, size, color);
}

void gfx_draw_rect_outline(int x, int y, int width, int height, uint32_t color)
{
    gfxdrv_draw_rect_outline(x, y, width, height, color);
}

void gfx_draw_circle(int cx, int cy, int radius, uint32_t color)
{
    gfxdrv_draw_circle(cx, cy, radius, color);
}

void gfx_fill_circle(int cx, int cy, int radius, uint32_t color)
{
    gfxdrv_fill_circle(cx, cy, radius, color);
}

void gfx_draw_char(int x, int y, char character, uint32_t color)
{
    gfxdrv_draw_char(x, y, character, color);
}

void gfx_draw_char_bg(int x, int y, char character, uint32_t foreground, uint32_t background)
{
    gfxdrv_draw_char_bg(x, y, character, foreground, background);
}

void gfx_draw_text(int x, int y, const char *text, uint32_t color)
{
    gfxdrv_draw_text(x, y, text, color);
}

int gfx_draw_image(int x, int y, const GfxImage *image)
{
    return gfxdrv_draw_image(x, y, image);
}

void gfx_blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y)
{
    gfxdrv_blit(src_x, src_y, width, height, dst_x, dst_y);
}

void gfx_set_clip(int x, int y, int width, int height)
{
    gfxdrv_set_clip(x, y, width, height);
}

void gfx_reset_clip(void)
{
    gfxdrv_reset_clip();
}

void gfx_get_clip(int *x, int *y, int *width, int *height)
{
    gfxdrv_get_clip(x, y, width, height);
}

void gfx_clear_screen(uint32_t color)
{
    gfx_clear(color);
}

void gfx_print_char(int x, int y, char character, uint32_t color)
{
    gfx_draw_char(x, y, character, color);
}

void gfx_print_char_gr_cl(char character, uint32_t color)
{
    if (character == '\n') {
        console_x = 0;
        console_y += 16;
        if (console_y + 16 > (int)gfx_height())
            console_y = 0;
        return;
    }

    if (character == '\b') {
        if (console_x >= 8)
            console_x -= 8;
        gfx_draw_rect(console_x, console_y, 8, 16, 0x00000000u);
        return;
    }

    gfx_draw_char(console_x, console_y, character, color);
    console_x += 8;
    if (console_x + 8 > (int)gfx_width()) {
        console_x = 0;
        console_y += 16;
    }
    if (console_y + 16 > (int)gfx_height())
        console_y = 0;
}

void gfx_print_gr_cl(const char *text, uint32_t color)
{
    if (!text)
        return;
    while (*text)
        gfx_print_char_gr_cl(*text++, color);
}
