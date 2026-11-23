#include "../../include/desktop.h"
#include "../../include/gfx.h"
#include <stdint.h>

#define CURSOR_SIZE 10

static volatile int desktop_dirty = 1;
static int cursor_x = -1;
static int cursor_y = -1;
static int desktop_started = 0;
static int cursor_saved = 0;
static uint32_t cursor_underlay[CURSOR_SIZE * CURSOR_SIZE];

static uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(uint32_t flags) {
    if (flags & 0x200u)
        __asm__ volatile("sti" : : : "memory");
}

static void cursor_restore(void) {
    if (!cursor_saved)
        return;

    for (int y = 0; y < CURSOR_SIZE; ++y) {
        for (int x = 0; x < CURSOR_SIZE; ++x) {
            int sx = cursor_x + x;
            int sy = cursor_y + y;
            if (sx >= 0 && sx < (int)gfx_width() && sy >= 0 && sy < (int)gfx_height())
                gfx_draw_pixel(sx, sy, cursor_underlay[y * CURSOR_SIZE + x]);
        }
    }

    cursor_saved = 0;
}

static void cursor_draw_saved(int x, int y) {
    for (int cy = 0; cy < CURSOR_SIZE; ++cy) {
        for (int cx = 0; cx < CURSOR_SIZE; ++cx) {
            int sx = x + cx;
            int sy = y + cy;
            int index = cy * CURSOR_SIZE + cx;

            if (sx < 0 || sx >= (int)gfx_width() || sy < 0 || sy >= (int)gfx_height())
                continue;

            cursor_underlay[index] = gfx_get_pixel(sx, sy);
            gfx_draw_pixel(sx, sy, 0x0000FF00);
        }
    }

    cursor_saved = 1;
}

static int take_desktop_dirty(void) {
    uint32_t flags = irq_save();
    int dirty = desktop_dirty;
    desktop_dirty = 0;
    irq_restore(flags);
    return dirty;
}

void desktop_invalidate(void) {
    uint32_t flags;
    wm_invalidate_all();
    flags = irq_save();
    desktop_dirty = 1;
    irq_restore(flags);
}

void desktop_invalidate_rect(int x, int y, int width, int height) {
    wm_invalidate_rect(x, y, width, height);
    {
        uint32_t flags = irq_save();
        desktop_dirty = 1;
        irq_restore(flags);
    }
}

void desktop_run(void) {
    if (desktop_started)
        return;

    desktop_init();
    if (task_create(desktop_update, 0) != 0)
        desktop_started = 1;
}

void desktop_update(void *arg) {
    (void)arg;

    for (;;) {
        int x = get_mouse_x();
        int y = get_mouse_y();
        wm_handle_mouse(x, y, get_mouse_buttons());
        int dirty = take_desktop_dirty();

        if (gfx_cursor_hardware_ready()) {
            if (dirty) {
                wm_draw_all();
                gfx_present();
                wm_render_gl_windows();
            }
            if (x != cursor_x || y != cursor_y)
                gfx_cursor_move(x, y);
            cursor_x = x;
            cursor_y = y;
            task_sleep(10);
            continue;
        }

        if (dirty) {
            cursor_restore();
            wm_draw_all();
            cursor_saved = 0;
        }

        if (dirty || x != cursor_x || y != cursor_y) {
            if (!dirty)
                cursor_restore();

            cursor_draw_saved(x, y);
            cursor_x = x;
            cursor_y = y;
            gfx_present();
            wm_render_gl_windows();
        }

        task_sleep(10);
    }
}

void desktop_init(void) {
    gfx_clear(0x000000FF);
    desktop_invalidate();
}

void draw_cursor(void *arg) {
    int x;
    int y;

    (void)arg;
    x = get_mouse_x();
    y = get_mouse_y();
    gfx_draw_rect(x, y, CURSOR_SIZE, CURSOR_SIZE, 0x0000FF00);
}
