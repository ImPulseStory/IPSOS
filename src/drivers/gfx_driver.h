#ifndef IPSOS_GFX_DRIVER_H
#define IPSOS_GFX_DRIVER_H

#include "gfx.h"
#include <stdint.h>

int gfxdrv_init(void);
int gfxdrv_ready(void);
int gfxdrv_present(void);
uint32_t gfxdrv_error(void);
uint32_t gfxdrv_width(void);
uint32_t gfxdrv_height(void);
void gfxdrv_clear(uint32_t color);
void gfxdrv_draw_pixel(int x, int y, uint32_t color);
uint32_t gfxdrv_get_pixel(int x, int y);
void gfxdrv_draw_hline(int x, int y, int width, uint32_t color);
void gfxdrv_draw_vline(int x, int y, int height, uint32_t color);
void gfxdrv_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfxdrv_draw_rect(int x, int y, int width, int height, uint32_t color);
void gfxdrv_draw_rect_outline(int x, int y, int width, int height, uint32_t color);
void gfxdrv_draw_circle(int cx, int cy, int radius, uint32_t color);
void gfxdrv_fill_circle(int cx, int cy, int radius, uint32_t color);
void gfxdrv_draw_char(int x, int y, char character, uint32_t color);
void gfxdrv_draw_char_bg(int x, int y, char character, uint32_t foreground, uint32_t background);
void gfxdrv_draw_text(int x, int y, const char *text, uint32_t color);
int gfxdrv_draw_image(int x, int y, const GfxImage *image);
void gfxdrv_blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y);
void gfxdrv_set_clip(int x, int y, int width, int height);
void gfxdrv_reset_clip(void);
void gfxdrv_get_clip(int *x, int *y, int *width, int *height);

#endif
