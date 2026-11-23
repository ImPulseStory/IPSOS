#include "../../include/desktop.h"
#include "../../include/gfx.h"
#include <stdint.h>

#define WINDOW_BACKGROUND 0x00101010u
#define WINDOW_TITLEBAR   0x00263A52u
#define WINDOW_BORDER     0x00AAB7C4u
#define WINDOW_TITLE      0x00FFFFFFu
#define WINDOW_CLOSE      0x00C84A4Au
#define DESKTOP_BACKGROUND 0x000000FFu
#define WM_TITLEBAR_HEIGHT 22

static Window_t windows[MAX_WINDOWS];
static Window_t *window_order[MAX_WINDOWS];
static int window_count;

#define WM_DIRTY_MAX 16u
typedef struct WmDirtyRect { int x, y, width, height; } WmDirtyRect;
static WmDirtyRect wm_dirty_rects[WM_DIRTY_MAX];
static uint32_t wm_dirty_count;
static int wm_dirty_full = 1;

static uint32_t wm_irq_save(void)
{
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void wm_irq_restore(uint32_t flags)
{
    if (flags & 0x200u) __asm__ volatile("sti" : : : "memory");
}

void wm_invalidate_all(void)
{
    uint32_t flags = wm_irq_save();
    wm_dirty_full = 1;
    wm_dirty_count = 0;
    wm_irq_restore(flags);
}

void wm_invalidate_rect(int x, int y, int width, int height)
{
    int64_t left = x, top = y;
    int64_t right = (int64_t)x + width;
    int64_t bottom = (int64_t)y + height;
    int sw = (int)gfx_width();
    int sh = (int)gfx_height();

    uint32_t flags;
    if (width <= 0 || height <= 0 || sw <= 0 || sh <= 0) return;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > sw) right = sw;
    if (bottom > sh) bottom = sh;
    if (left >= right || top >= bottom) return;

    flags = wm_irq_save();
    if (wm_dirty_full) { wm_irq_restore(flags); return; }
    for (uint32_t i = 0; i < wm_dirty_count;) {
        WmDirtyRect *r = &wm_dirty_rects[i];
        int64_t rr = (int64_t)r->x + r->width;
        int64_t rb = (int64_t)r->y + r->height;
        if (left <= rr && right >= r->x && top <= rb && bottom >= r->y) {
            if (r->x < left) left = r->x;
            if (r->y < top) top = r->y;
            if (rr > right) right = rr;
            if (rb > bottom) bottom = rb;
            wm_dirty_rects[i] = wm_dirty_rects[--wm_dirty_count];
            i = 0;
        } else ++i;
    }

    if (wm_dirty_count == WM_DIRTY_MAX) {
        wm_dirty_full = 1;
        wm_dirty_count = 0;
        wm_irq_restore(flags);
        return;
    }
    wm_dirty_rects[wm_dirty_count++] = (WmDirtyRect){
        (int)left, (int)top, (int)(right - left), (int)(bottom - top)
    };
    wm_irq_restore(flags);
}

static int wm_rect_intersects(int ax, int ay, int aw, int ah,
                              int bx, int by, int bw, int bh)
{
    return aw > 0 && ah > 0 && bw > 0 && bh > 0 &&
           ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void wm_draw_in_region(Window_t *window, int rx, int ry, int rw, int rh)
{
    int close_x;
    int tx1, ty1, tx2, ty2;

    if (!window || !window->visible || window->w <= 0 || window->h <= 0) return;

    gfx_draw_rect(window->x, window->y, window->w, window->h, WINDOW_BACKGROUND);
    gfx_draw_rect(window->x, window->y, window->w, WM_TITLEBAR_HEIGHT, WINDOW_TITLEBAR);
    gfx_draw_rect_outline(window->x, window->y, window->w, window->h, WINDOW_BORDER);

    if (window->title && window->w > 28) {
        tx1 = window->x + 6;
        ty1 = window->y + 1;
        tx2 = window->x + window->w - 22;
        ty2 = window->y + WM_TITLEBAR_HEIGHT - 1;
        if (tx1 < rx) tx1 = rx;
        if (ty1 < ry) ty1 = ry;
        if (tx2 > rx + rw) tx2 = rx + rw;
        if (ty2 > ry + rh) ty2 = ry + rh;
        if (tx1 < tx2 && ty1 < ty2) {
            gfx_set_clip(tx1, ty1, tx2 - tx1, ty2 - ty1);
            gfx_draw_text(window->x + 8, window->y + 3, window->title, WINDOW_TITLE);
            gfx_set_clip(rx, ry, rw, rh);
        }
    }

    close_x = window->x + window->w - 16;
    if (window->w >= 40) {
        gfx_draw_rect(close_x, window->y + 5, 10, 10, WINDOW_CLOSE);
        gfx_draw_line(close_x + 2, window->y + 7, close_x + 7, window->y + 12, WINDOW_TITLE);
        gfx_draw_line(close_x + 7, window->y + 7, close_x + 2, window->y + 12, WINDOW_TITLE);
    }
}

static void wm_paint_region(int x, int y, int width, int height)
{
    gfx_set_clip(x, y, width, height);
    gfx_draw_rect(x, y, width, height, DESKTOP_BACKGROUND);

    for (int i = 0; i < window_count; ++i) {
        Window_t *window = window_order[i];
        int client_x, client_y, client_w, client_h;
        int cx1, cy1, cx2, cy2;

        if (!window || !window->visible ||
            !wm_rect_intersects(x, y, width, height, window->x, window->y, window->w, window->h))
            continue;

        wm_draw_in_region(window, x, y, width, height);
        if (!window->render_content) continue;

        client_x = window->x + 1;
        client_y = window->y + WM_TITLEBAR_HEIGHT;
        client_w = window->w - 2;
        client_h = window->h - WM_TITLEBAR_HEIGHT - 1;
        cx1 = client_x > x ? client_x : x;
        cy1 = client_y > y ? client_y : y;
        cx2 = client_x + client_w < x + width ? client_x + client_w : x + width;
        cy2 = client_y + client_h < y + height ? client_y + client_h : y + height;
        if (cx1 < cx2 && cy1 < cy2) {
            gfx_set_clip(cx1, cy1, cx2 - cx1, cy2 - cy1);
            window->render_content(window);
            gfx_set_clip(x, y, width, height);
        }
    }
    gfx_reset_clip();
}

/* GPU-backed window callbacks run only after the software framebuffer has
   been uploaded. They render into the shared VirGL display resource, so they
   must not be called from wm_paint_region before gfx_present(). */
void wm_render_gl_windows(void)
{
    for (int i = 0; i < window_count; ++i) {
        Window_t *window = window_order[i];
        if (window && window->visible && !window->closed && window->render_gl_content)
            window->render_gl_content(window);
    }
}

void wm_draw_all(void)
{
    WmDirtyRect snapshot[WM_DIRTY_MAX];
    uint32_t flags = wm_irq_save();
    uint32_t count = wm_dirty_count;
    int full = wm_dirty_full;

    if (count > WM_DIRTY_MAX) count = WM_DIRTY_MAX;
    for (uint32_t i = 0; i < count; ++i) snapshot[i] = wm_dirty_rects[i];
    wm_dirty_count = 0;
    wm_dirty_full = 0;
    wm_irq_restore(flags);

    if (full) {
        wm_paint_region(0, 0, (int)gfx_width(), (int)gfx_height());
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        WmDirtyRect r = snapshot[i];
        wm_paint_region(r.x, r.y, r.width, r.height);
    }
}

void wm_draw(Window_t *window)
{
    gfx_set_clip(0, 0, (int)gfx_width(), (int)gfx_height());
    wm_draw_in_region(window, 0, 0, (int)gfx_width(), (int)gfx_height());
    gfx_reset_clip();
}

void wm_focus(Window_t *window)
{
    int index = -1;

    if (!window)
        return;

    for (int i = 0; i < window_count; ++i) {
        if (window_order[i] == window) {
            index = i;
            break;
        }
    }

    if (index < 0 || index == window_count - 1)
        return;

    for (int i = index; i < window_count - 1; ++i)
        window_order[i] = window_order[i + 1];

    window_order[window_count - 1] = window;
    for (int i = 0; i < window_count; ++i)
        window_order[i]->z = i;

    desktop_invalidate();
}

void wm_close(Window_t *window)
{
    if (!window)
        return;

    window->visible = 0;
    window->closed = 1;
    desktop_invalidate_rect(window->x, window->y, window->w, window->h);
}

void wm_hide(Window_t *window)
{
    if (!window) return;
    window->visible = 0;
    desktop_invalidate_rect(window->x, window->y, window->w, window->h);
}

void wm_show(Window_t *window)
{
    if (!window) return;
    window->visible = 1;
    window->closed = 0;
    wm_focus(window);
    desktop_invalidate_rect(window->x, window->y, window->w, window->h);
}

void wm_set_title(Window_t *window, const char *title)
{
    if (!window) return;
    window->title = title;
    desktop_invalidate_rect(window->x, window->y, window->w, WM_TITLEBAR_HEIGHT);
}

void wm_destroy(Window_t *window)
{
    int index = -1;
    int old_x, old_y, old_w, old_h;
    uint32_t flags;
    if (!window) return;

    flags = wm_irq_save();
    for (int i = 0; i < window_count; ++i) {
        if (window_order[i] == window) { index = i; break; }
    }
    if (index < 0) { wm_irq_restore(flags); return; }

    old_x = window->x; old_y = window->y;
    old_w = window->w; old_h = window->h;
    for (int i = index; i < window_count - 1; ++i)
        window_order[i] = window_order[i + 1];
    --window_count;
    window_order[window_count] = 0;
    window->visible = 0;
    window->closed = 1;
    window->render_content = 0;
    window->render_gl_content = 0;
    window->title = 0;
    for (int i = 0; i < window_count; ++i)
        window_order[i]->z = i;
    wm_irq_restore(flags);
    desktop_invalidate_rect(old_x, old_y, old_w, old_h);
}

Window_t *wm_create(int x, int y, int w, int h, const char *title, int visible)
{
    Window_t *window;
    int slot = -1;
    int screen_w = (int)gfx_width();
    int screen_h = (int)gfx_height();

    if (window_count >= MAX_WINDOWS || w < 40 || h < WM_TITLEBAR_HEIGHT + 8 ||
        screen_w == 0 || screen_h == 0)
        return 0;

    if (w > screen_w)
        w = screen_w;
    if (h > screen_h)
        h = screen_h;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x > screen_w - w)
        x = screen_w - w;
    if (y > screen_h - h)
        y = screen_h - h;

    for (int candidate = 0; candidate < MAX_WINDOWS; ++candidate) {
        int used = 0;
        for (int i = 0; i < window_count; ++i) {
            if (window_order[i] == &windows[candidate]) {
                used = 1;
                break;
            }
        }
        if (!used) { slot = candidate; break; }
    }
    if (slot < 0)
        return 0;

    window = &windows[slot];
    window->x = x;
    window->y = y;
    window->w = w;
    window->h = h;
    window->title = title;
    window->z = window_count;
    window->visible = visible ? 1 : 0;
    window->closed = 0;
    window->render_content = 0;
    window->render_gl_content = 0;

    window_order[window_count] = window;
    ++window_count;

    if (window->visible)
        wm_focus(window);

    desktop_invalidate_rect(window->x, window->y, window->w, window->h);
    return window;
}


void wm_set_renderer(Window_t *window, WindowRenderFn renderer)
{
    if (!window)
        return;

    window->render_content = renderer;
    desktop_invalidate_rect(window->x + 1, window->y + WM_TITLEBAR_HEIGHT,
                            window->w - 2, window->h - WM_TITLEBAR_HEIGHT - 1);
}

void wm_resize(Window_t *window, int w, int h)
{
    if (!window || w < 40 || h < WM_TITLEBAR_HEIGHT + 8)
        return;

    if (w > (int)gfx_width())
        w = (int)gfx_width();
    if (h > (int)gfx_height())
        h = (int)gfx_height();

    window->w = w;
    window->h = h;
    wm_set_pos(window, window->x, window->y);
    desktop_invalidate();
}

void wm_set_pos(Window_t *window, int x, int y)
{
    if (!window)
        return;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x > (int)gfx_width() - window->w)
        x = (int)gfx_width() - window->w;
    if (y > (int)gfx_height() - window->h)
        y = (int)gfx_height() - window->h;

    if (window->x == x && window->y == y)
        return;

    {
        int old_x = window->x;
        int old_y = window->y;
        window->x = x;
        window->y = y;
        desktop_invalidate_rect(old_x, old_y, window->w, window->h);
        desktop_invalidate_rect(window->x, window->y, window->w, window->h);
    }
}

void wm_unvisible(Window_t *window)
{
    if (window)
        wm_close(window);
}


void wm_handle_mouse(int x, int y, int buttons)
{
    static int previous_left;
    static Window_t *drag_window;
    static int drag_offset_x;
    static int drag_offset_y;
    int left = (buttons & 1) != 0;

    if (left && !previous_left) {
        drag_window = 0;

        for (int i = window_count - 1; i >= 0; --i) {
            Window_t *window = window_order[i];
            if (!window || !window->visible)
                continue;
            if (x < window->x || y < window->y ||
                x >= window->x + window->w || y >= window->y + window->h)
                continue;

            wm_focus(window);
            if (y < window->y + WM_TITLEBAR_HEIGHT) {
                if (x >= window->x + window->w - 20 &&
                    x < window->x + window->w - 4 &&
                    y >= window->y + 3 && y < window->y + 18) {
                    wm_close(window);
                } else {
                    drag_window = window;
                    drag_offset_x = x - window->x;
                    drag_offset_y = y - window->y;
                }
            }
            break;
        }
    }

    if (left && drag_window)
        wm_set_pos(drag_window, x - drag_offset_x, y - drag_offset_y);

    if (!left)
        drag_window = 0;

    previous_left = left;
}
