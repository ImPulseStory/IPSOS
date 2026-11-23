#include "gfx_driver.h"
#include "../include/virtio_gpu.h"
#include "../include/font.h"
#include <stdint.h>
#include <stddef.h>

static uint32_t *gfx_framebuffer;
static uint32_t gfx_stride;
static uint32_t gfx_screen_width;
static uint32_t gfx_screen_height;
static int clip_left;
static int clip_top;
static int clip_right;
static int clip_bottom;

#define GFX_DIRTY_MAX 16u
typedef struct GfxDirtyRect { int x, y, width, height; } GfxDirtyRect;
static GfxDirtyRect dirty_rects[GFX_DIRTY_MAX];
static uint32_t dirty_count;
static int dirty_full;

static void gfxdrv_mark_dirty(int x, int y, int width, int height)
{
    int64_t left = x, top = y;
    int64_t right = (int64_t)x + width;
    int64_t bottom = (int64_t)y + height;

    if (dirty_full || width <= 0 || height <= 0 || !gfx_screen_width || !gfx_screen_height) return;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > gfx_screen_width) right = gfx_screen_width;
    if (bottom > gfx_screen_height) bottom = gfx_screen_height;
    if (left >= right || top >= bottom) return;

    for (uint32_t i = 0; i < dirty_count;) {
        GfxDirtyRect *r = &dirty_rects[i];
        int64_t rr = (int64_t)r->x + r->width;
        int64_t rb = (int64_t)r->y + r->height;
        if (left <= rr && right >= r->x && top <= rb && bottom >= r->y) {
            if (r->x < left) left = r->x;
            if (r->y < top) top = r->y;
            if (rr > right) right = rr;
            if (rb > bottom) bottom = rb;
            dirty_rects[i] = dirty_rects[--dirty_count];
            i = 0;
        } else ++i;
    }

    if (dirty_count == GFX_DIRTY_MAX) {
        for (uint32_t i = 0; i < dirty_count; ++i) {
            GfxDirtyRect *r = &dirty_rects[i];
            if (r->x < left) left = r->x;
            if (r->y < top) top = r->y;
            if ((int64_t)r->x + r->width > right) right = (int64_t)r->x + r->width;
            if ((int64_t)r->y + r->height > bottom) bottom = (int64_t)r->y + r->height;
        }
        dirty_count = 0;
    }
    dirty_rects[dirty_count++] = (GfxDirtyRect){(int)left, (int)top, (int)(right-left), (int)(bottom-top)};
}

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static uint32_t out_code(int64_t x, int64_t y)
{
    uint32_t code = 0;

    if (x < clip_left)
        code |= 1u;
    else if (x >= clip_right)
        code |= 2u;
    if (y < clip_top)
        code |= 4u;
    else if (y >= clip_bottom)
        code |= 8u;
    return code;
}

int gfxdrv_init(void)
{
    if (virtio_gpu_init(&gfx_framebuffer, &gfx_screen_width, &gfx_screen_height) != 0 ||
        !gfx_framebuffer || !gfx_screen_width || !gfx_screen_height) {
        gfx_framebuffer = 0;
        gfx_stride = 0;
        gfx_screen_width = 0;
        gfx_screen_height = 0;
        return -1;
    }

    gfx_stride = gfx_screen_width;
    dirty_count = 0;
    dirty_full = 1;
    gfxdrv_reset_clip();
    return 0;
}

int gfxdrv_present(void)
{
    if (!gfx_framebuffer) return -1;
    if (dirty_full) {
        int result = virtio_gpu_present();
        if (result == 0) dirty_full = 0;
        return result;
    }
    for (uint32_t i = 0; i < dirty_count; ++i) {
        GfxDirtyRect *r = &dirty_rects[i];
        if (virtio_gpu_present_rect((uint32_t)r->x, (uint32_t)r->y,
                                    (uint32_t)r->width, (uint32_t)r->height) != 0) return -1;
    }
    dirty_count = 0;
    return 0;
}

uint32_t gfxdrv_error(void)
{
    return virtio_gpu_last_error();
}

int gfxdrv_ready(void)
{
    return gfx_framebuffer != 0;
}

uint32_t gfxdrv_width(void)
{
    return gfx_screen_width;
}

uint32_t gfxdrv_height(void)
{
    return gfx_screen_height;
}

void gfxdrv_reset_clip(void)
{
    clip_left = 0;
    clip_top = 0;
    clip_right = (int)gfx_screen_width;
    clip_bottom = (int)gfx_screen_height;
}

void gfxdrv_get_clip(int *x, int *y, int *width, int *height)
{
    if (x) *x = clip_left;
    if (y) *y = clip_top;
    if (width) *width = clip_right - clip_left;
    if (height) *height = clip_bottom - clip_top;
}

void gfxdrv_set_clip(int x, int y, int width, int height)
{
    int64_t right;
    int64_t bottom;

    if (width <= 0 || height <= 0 || !gfx_framebuffer) {
        clip_left = 0;
        clip_top = 0;
        clip_right = 0;
        clip_bottom = 0;
        return;
    }

    right = (int64_t)x + width;
    bottom = (int64_t)y + height;

    clip_left = max_int(x, 0);
    clip_top = max_int(y, 0);
    clip_right = (int)(right < (int64_t)gfx_screen_width ? right : gfx_screen_width);
    clip_bottom = (int)(bottom < (int64_t)gfx_screen_height ? bottom : gfx_screen_height);

    if (clip_right < clip_left)
        clip_right = clip_left;
    if (clip_bottom < clip_top)
        clip_bottom = clip_top;
}

void gfxdrv_clear(uint32_t color)
{
    if (!gfx_framebuffer)
        return;

    for (uint32_t y = 0; y < gfx_screen_height; ++y) {
        uint32_t *row = gfx_framebuffer + y * gfx_stride;
        for (uint32_t x = 0; x < gfx_screen_width; ++x)
            row[x] = color;
    }
    dirty_count = 0;
    dirty_full = 1;
}

void gfxdrv_draw_pixel(int x, int y, uint32_t color)
{
    if (!gfx_framebuffer || x < clip_left || x >= clip_right ||
        y < clip_top || y >= clip_bottom)
        return;

    gfx_framebuffer[(uint32_t)y * gfx_stride + (uint32_t)x] = color;
    gfxdrv_mark_dirty(x, y, 1, 1);
}

uint32_t gfxdrv_get_pixel(int x, int y)
{
    if (!gfx_framebuffer || x < 0 || y < 0 ||
        (uint32_t)x >= gfx_screen_width || (uint32_t)y >= gfx_screen_height)
        return 0;

    return gfx_framebuffer[(uint32_t)y * gfx_stride + (uint32_t)x];
}

void gfxdrv_draw_rect(int x, int y, int width, int height, uint32_t color)
{
    int64_t right;
    int64_t bottom;
    int left;
    int top;
    int draw_right;
    int draw_bottom;

    if (!gfx_framebuffer || width <= 0 || height <= 0 ||
        clip_left >= clip_right || clip_top >= clip_bottom)
        return;

    right = (int64_t)x + width;
    bottom = (int64_t)y + height;
    left = max_int(x, clip_left);
    top = max_int(y, clip_top);
    draw_right = (int)(right < clip_right ? right : clip_right);
    draw_bottom = (int)(bottom < clip_bottom ? bottom : clip_bottom);

    if (left >= draw_right || top >= draw_bottom)
        return;

    for (int row = top; row < draw_bottom; ++row) {
        uint32_t *dst = gfx_framebuffer + (uint32_t)row * gfx_stride + (uint32_t)left;
        for (int col = left; col < draw_right; ++col)
            *dst++ = color;
    }
    gfxdrv_mark_dirty(left, top, draw_right - left, draw_bottom - top);
}

void gfxdrv_draw_hline(int x, int y, int width, uint32_t color)
{
    gfxdrv_draw_rect(x, y, width, 1, color);
}

void gfxdrv_draw_vline(int x, int y, int height, uint32_t color)
{
    gfxdrv_draw_rect(x, y, 1, height, color);
}

void gfxdrv_draw_rect_outline(int x, int y, int width, int height, uint32_t color)
{
    int64_t right;
    int64_t bottom;

    if (width <= 0 || height <= 0)
        return;

    right = (int64_t)x + width - 1;
    bottom = (int64_t)y + height - 1;
    gfxdrv_draw_hline(x, y, width, color);
    if (height > 1 && bottom >= -2147483648LL && bottom <= 2147483647LL)
        gfxdrv_draw_hline(x, (int)bottom, width, color);
    gfxdrv_draw_vline(x, y, height, color);
    if (width > 1 && right >= -2147483648LL && right <= 2147483647LL)
        gfxdrv_draw_vline((int)right, y, height, color);
}

void gfxdrv_draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    uint32_t c0;
    uint32_t c1;
    int clipped = 0;

    if (!gfx_framebuffer || clip_left >= clip_right || clip_top >= clip_bottom)
        return;

    for (int attempt = 0; attempt < 8; ++attempt) {
        int64_t x;
        int64_t y;
        uint32_t out;

        c0 = out_code(x0, y0);
        c1 = out_code(x1, y1);
        if (!(c0 | c1)) {
            clipped = 1;
            break;
        }
        if (c0 & c1)
            return;

        out = c0 ? c0 : c1;
        if (out & 4u) {
            if (y1 == y0)
                return;
            y = clip_top;
            x = (int64_t)x0 + ((int64_t)x1 - x0) * (y - y0) / ((int64_t)y1 - y0);
        } else if (out & 8u) {
            if (y1 == y0)
                return;
            y = clip_bottom - 1;
            x = (int64_t)x0 + ((int64_t)x1 - x0) * (y - y0) / ((int64_t)y1 - y0);
        } else if (out & 2u) {
            if (x1 == x0)
                return;
            x = clip_right - 1;
            y = (int64_t)y0 + ((int64_t)y1 - y0) * (x - x0) / ((int64_t)x1 - x0);
        } else {
            if (x1 == x0)
                return;
            x = clip_left;
            y = (int64_t)y0 + ((int64_t)y1 - y0) * (x - x0) / ((int64_t)x1 - x0);
        }

        if (out == c0) {
            x0 = (int)x;
            y0 = (int)y;
        } else {
            x1 = (int)x;
            y1 = (int)y;
        }
    }

    if (!clipped)
        return;

    {
        int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
        int sx = x0 < x1 ? 1 : -1;
        int dy = y1 >= y0 ? y0 - y1 : y1 - y0;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        for (;;) {
            gfxdrv_draw_pixel(x0, y0, color);
            if (x0 == x1 && y0 == y1)
                break;
            {
                int twice = err * 2;
                if (twice >= dy) {
                    err += dy;
                    x0 += sx;
                }
                if (twice <= dx) {
                    err += dx;
                    y0 += sy;
                }
            }
        }
    }
}

static void gfxdrv_plot64(int64_t x, int64_t y, uint32_t color)
{
    if (x < -2147483648LL || x > 2147483647LL ||
        y < -2147483648LL || y > 2147483647LL)
        return;
    gfxdrv_draw_pixel((int)x, (int)y, color);
}

static void gfxdrv_hspan64(int64_t left, int64_t y, int64_t right, uint32_t color)
{
    if (!gfx_framebuffer || y < clip_top || y >= clip_bottom)
        return;
    if (left < clip_left)
        left = clip_left;
    if (right >= clip_right)
        right = clip_right - 1;
    if (left > right)
        return;
    for (int64_t x = left; x <= right; ++x)
        gfxdrv_draw_pixel((int)x, (int)y, color);
}

void gfxdrv_draw_circle(int cx, int cy, int radius, uint32_t color)
{
    int x;
    int y;
    int error;

    if (radius < 0 || radius > 4096 || !gfx_framebuffer)
        return;
    if ((int64_t)cx + radius < clip_left || (int64_t)cx - radius >= clip_right ||
        (int64_t)cy + radius < clip_top || (int64_t)cy - radius >= clip_bottom)
        return;

    x = radius;
    y = 0;
    error = 1 - radius;

    while (x >= y) {
        gfxdrv_plot64((int64_t)cx + x, (int64_t)cy + y, color);
        gfxdrv_plot64((int64_t)cx + y, (int64_t)cy + x, color);
        gfxdrv_plot64((int64_t)cx - y, (int64_t)cy + x, color);
        gfxdrv_plot64((int64_t)cx - x, (int64_t)cy + y, color);
        gfxdrv_plot64((int64_t)cx - x, (int64_t)cy - y, color);
        gfxdrv_plot64((int64_t)cx - y, (int64_t)cy - x, color);
        gfxdrv_plot64((int64_t)cx + y, (int64_t)cy - x, color);
        gfxdrv_plot64((int64_t)cx + x, (int64_t)cy - y, color);
        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

void gfxdrv_fill_circle(int cx, int cy, int radius, uint32_t color)
{
    int x;
    int y;
    int error;

    if (radius < 0 || radius > 4096 || !gfx_framebuffer)
        return;
    if ((int64_t)cx + radius < clip_left || (int64_t)cx - radius >= clip_right ||
        (int64_t)cy + radius < clip_top || (int64_t)cy - radius >= clip_bottom)
        return;

    x = radius;
    y = 0;
    error = 1 - radius;

    while (x >= y) {
        gfxdrv_hspan64((int64_t)cx - x, (int64_t)cy + y, (int64_t)cx + x, color);
        if (y)
            gfxdrv_hspan64((int64_t)cx - x, (int64_t)cy - y, (int64_t)cx + x, color);
        if (x != y) {
            gfxdrv_hspan64((int64_t)cx - y, (int64_t)cy + x, (int64_t)cx + y, color);
            if (x)
                gfxdrv_hspan64((int64_t)cx - y, (int64_t)cy - x, (int64_t)cx + y, color);
        }
        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

void gfxdrv_draw_char(int x, int y, char character, uint32_t color)
{
    const uint8_t *glyph = font8x16[(uint8_t)character];

    if (!gfx_framebuffer)
        return;

    for (int row = 0; row < 16; ++row) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; ++col) {
            if (bits & (uint8_t)(1u << (7 - col)))
                gfxdrv_plot64((int64_t)x + col, (int64_t)y + row, color);
        }
    }
}

void gfxdrv_draw_char_bg(int x, int y, char character, uint32_t foreground, uint32_t background)
{
    gfxdrv_draw_rect(x, y, 8, 16, background);
    gfxdrv_draw_char(x, y, character, foreground);
}

void gfxdrv_draw_text(int x, int y, const char *text, uint32_t color)
{
    int origin_x = x;

    if (!text)
        return;

    while (*text) {
        if (*text == '\n') {
            x = origin_x;
            if (y > 2147483631)
                return;
            y += 16;
        } else if (*text == '\t') {
            if (x > 2147483615)
                return;
            x += 32;
        } else {
            gfxdrv_draw_char(x, y, *text, color);
            if (x > 2147483639)
                return;
            x += 8;
        }
        ++text;
        if (y >= (int)gfx_screen_height + 16)
            return;
    }
}

int gfxdrv_draw_image(int x, int y, const GfxImage *image)
{
    int64_t right;
    int64_t bottom;
    int left;
    int top;
    int draw_right;
    int draw_bottom;

    if (!gfx_framebuffer || !image || !image->pixels || !image->width || !image->height ||
        (uint64_t)image->width * image->height > 0xFFFFFFFFu)
        return -1;

    right = (int64_t)x + image->width;
    bottom = (int64_t)y + image->height;
    left = max_int(x, clip_left);
    top = max_int(y, clip_top);
    draw_right = (int)(right < clip_right ? right : clip_right);
    draw_bottom = (int)(bottom < clip_bottom ? bottom : clip_bottom);

    if (left >= draw_right || top >= draw_bottom)
        return 0;

    for (int py = top; py < draw_bottom; ++py) {
        uint32_t source_y = (uint32_t)((int64_t)py - y);
        const uint32_t *source = image->pixels + source_y * image->width + (uint32_t)((int64_t)left - x);
        uint32_t *destination = gfx_framebuffer + (uint32_t)py * gfx_stride + (uint32_t)left;

        for (int px = left; px < draw_right; ++px) {
            uint32_t color = *source++;
            if (!(image->flags & GFX_IMAGE_COLORKEY) || color != image->transparent_color)
                *destination = color;
            ++destination;
        }
    }
    gfxdrv_mark_dirty(left, top, draw_right - left, draw_bottom - top);

    return 0;
}

void gfxdrv_blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y)
{
    int64_t x_begin = 0;
    int64_t y_begin = 0;
    int64_t x_end;
    int64_t y_end;
    int64_t y;
    int64_t y_stop;
    int64_t y_step;
    int64_t x_step;

    if (!gfx_framebuffer || width <= 0 || height <= 0)
        return;

    x_end = width;
    y_end = height;

    if (-(int64_t)src_x > x_begin)
        x_begin = -(int64_t)src_x;
    if ((int64_t)clip_left - dst_x > x_begin)
        x_begin = (int64_t)clip_left - dst_x;
    if (-(int64_t)src_y > y_begin)
        y_begin = -(int64_t)src_y;
    if ((int64_t)clip_top - dst_y > y_begin)
        y_begin = (int64_t)clip_top - dst_y;

    if ((int64_t)gfx_screen_width - src_x < x_end)
        x_end = (int64_t)gfx_screen_width - src_x;
    if ((int64_t)clip_right - dst_x < x_end)
        x_end = (int64_t)clip_right - dst_x;
    if ((int64_t)gfx_screen_height - src_y < y_end)
        y_end = (int64_t)gfx_screen_height - src_y;
    if ((int64_t)clip_bottom - dst_y < y_end)
        y_end = (int64_t)clip_bottom - dst_y;

    if (x_begin >= x_end || y_begin >= y_end)
        return;

    if ((int64_t)dst_y + y_begin > (int64_t)src_y + y_begin) {
        y = y_end - 1;
        y_stop = y_begin - 1;
        y_step = -1;
    } else {
        y = y_begin;
        y_stop = y_end;
        y_step = 1;
    }

    x_step = ((int64_t)dst_y + y_begin == (int64_t)src_y + y_begin && dst_x > src_x) ? -1 : 1;

    for (; y != y_stop; y += y_step) {
        int64_t x = x_step > 0 ? x_begin : x_end - 1;
        int64_t x_stop = x_step > 0 ? x_end : x_begin - 1;
        for (; x != x_stop; x += x_step) {
            uint32_t source_x = (uint32_t)((int64_t)src_x + x);
            uint32_t source_y = (uint32_t)((int64_t)src_y + y);
            uint32_t target_x = (uint32_t)((int64_t)dst_x + x);
            uint32_t target_y = (uint32_t)((int64_t)dst_y + y);
            uint32_t color = gfx_framebuffer[source_y * gfx_stride + source_x];
            gfx_framebuffer[target_y * gfx_stride + target_x] = color;
        }
    }
    gfxdrv_mark_dirty((int)((int64_t)dst_x + x_begin),
                      (int)((int64_t)dst_y + y_begin),
                      (int)(x_end - x_begin), (int)(y_end - y_begin));
}
