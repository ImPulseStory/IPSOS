#include "../include/window.h"
#include "../include/desktop.h"
#include "../include/gfx.h"
#include "../include/string.h"
#include "../include/task.h"

#define WINDOW_API_SLOTS MAX_WINDOWS
#define WINDOW_TITLE_CAP 64
#define WINDOW_TITLEBAR_HEIGHT 22

typedef struct {
    Window_t *wm_window;
    window_render_fn render;
    window_gl_render_fn gl_render;
    uint32_t owner_task;
    uint32_t generation;
    char title[WINDOW_TITLE_CAP];
} window_api_slot_t;

static window_api_slot_t slots[WINDOW_API_SLOTS];

static window_t window_make_handle(int index, const window_api_slot_t *slot)
{
    return (slot->generation << 8) | (uint32_t)(index + 1);
}

static window_api_slot_t *window_slot(window_t handle)
{
    uint32_t index_field = handle & 0xFFu;
    uint32_t generation = handle >> 8;
    uint32_t index;
    if (!index_field || index_field > WINDOW_API_SLOTS || !generation)
        return 0;
    index = index_field - 1u;
    if (!slots[index].wm_window || slots[index].generation != generation)
        return 0;
    return &slots[index];
}

static int window_slot_index(const window_api_slot_t *slot)
{
    return (int)(slot - slots);
}

static uint32_t window_irq_save(void)
{
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void window_irq_restore(uint32_t flags)
{
    if (flags & 0x200u) __asm__ volatile("sti" : : : "memory");
}

static void window_render_dispatch(Window_t *wm_window)
{
    window_api_slot_t *slot = 0;
    uint32_t flags = window_irq_save();
    for (int i = 0; i < WINDOW_API_SLOTS; ++i) {
        if (slots[i].wm_window == wm_window) { slot = &slots[i]; break; }
    }
    if (slot && slot->render) {
        int width = wm_window->w - 2;
        int height = wm_window->h - WINDOW_TITLEBAR_HEIGHT - 1;
        window_t handle = window_make_handle(window_slot_index(slot), slot);
        if (width < 0) width = 0;
        if (height < 0) height = 0;
        /* App text/code is loaded into a shared fixed ELF region; do not preempt
           this callback while its code is executing. */
        slot->render(handle, width, height);
    }
    window_irq_restore(flags);
}

window_t window_create(const window_config_t *config)
{
    Window_t *wm_window;
    window_api_slot_t *slot = 0;
    int slot_index = -1;
    int x = 40, y = 40, width = 320, height = 220, visible = 1;
    const char *title = "IPSOS Window";
    uint32_t flags;

    if (config) {
        if (config->title) title = config->title;
        x = config->x; y = config->y;
        width = config->width; height = config->height;
        visible = config->visible;
    }

    flags = window_irq_save();
    for (int i = 0; i < WINDOW_API_SLOTS; ++i) {
        if (!slots[i].wm_window) { slot = &slots[i]; slot_index = i; break; }
    }
    if (!slot) { window_irq_restore(flags); return 0; }

    wm_window = wm_create(x, y, width, height, "", visible);
    if (!wm_window) { window_irq_restore(flags); return 0; }

    slot->generation = (slot->generation + 1u) & 0x00FFFFFFu;
    if (!slot->generation) slot->generation = 1u;
    slot->wm_window = wm_window;
    slot->render = 0;
    slot->gl_render = 0;
    slot->owner_task = task_get_id();
    strncpy(slot->title, title, WINDOW_TITLE_CAP - 1);
    slot->title[WINDOW_TITLE_CAP - 1] = '\0';
    wm_set_title(wm_window, slot->title);

    {
        window_t handle = window_make_handle(slot_index, slot);
        window_irq_restore(flags);
        return handle;
    }
}

void window_destroy(window_t window)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (!slot) { window_irq_restore(flags); return; }
    wm_destroy(slot->wm_window);
    slot->wm_window = 0;
    slot->render = 0;
    slot->gl_render = 0;
    slot->owner_task = 0;
    slot->title[0] = '\0';
    window_irq_restore(flags);
}

void window_destroy_owner(uint32_t owner_task)
{
    uint32_t flags;
    if (!owner_task) return;
    flags = window_irq_save();
    for (int i = 0; i < WINDOW_API_SLOTS; ++i) {
        if (slots[i].wm_window && slots[i].owner_task == owner_task)
            window_destroy(window_make_handle(i, &slots[i]));
    }
    window_irq_restore(flags);
}

void window_show(window_t window)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (slot) wm_show(slot->wm_window);
    window_irq_restore(flags);
}

void window_hide(window_t window)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (slot) wm_hide(slot->wm_window);
    window_irq_restore(flags);
}

int window_is_open(window_t window)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    int result = slot && !slot->wm_window->closed;
    window_irq_restore(flags);
    return result;
}

void window_set_title(window_t window, const char *title)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (!slot || !title) { window_irq_restore(flags); return; }
    strncpy(slot->title, title, WINDOW_TITLE_CAP - 1);
    slot->title[WINDOW_TITLE_CAP - 1] = '\0';
    wm_set_title(slot->wm_window, slot->title);
    window_irq_restore(flags);
}

void window_set_position(window_t window, int x, int y)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (slot) wm_set_pos(slot->wm_window, x, y);
    window_irq_restore(flags);
}

void window_set_size(window_t window, int width, int height)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (slot) wm_resize(slot->wm_window, width, height);
    window_irq_restore(flags);
}

void window_get_client_size(window_t window, int *width, int *height)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (!slot) {
        if (width) *width = 0;
        if (height) *height = 0;
        window_irq_restore(flags);
        return;
    }
    if (width) *width = slot->wm_window->w - 2;
    if (height) *height = slot->wm_window->h - WINDOW_TITLEBAR_HEIGHT - 1;
    window_irq_restore(flags);
}

void window_set_render_callback(window_t window, window_render_fn callback)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (!slot) { window_irq_restore(flags); return; }
    slot->render = callback;
    wm_set_renderer(slot->wm_window, callback ? window_render_dispatch : 0);
    window_irq_restore(flags);
}

static void window_gl_render_dispatch(Window_t *wm_window)
{
    window_api_slot_t *slot = 0;
    uint32_t flags = window_irq_save();
    for (int i = 0; i < WINDOW_API_SLOTS; ++i) {
        if (slots[i].wm_window == wm_window) { slot = &slots[i]; break; }
    }
    if (slot && slot->gl_render) {
        int client_x = wm_window->x + 1;
        int client_y = wm_window->y + WINDOW_TITLEBAR_HEIGHT;
        int width = wm_window->w - 2;
        int height = wm_window->h - WINDOW_TITLEBAR_HEIGHT - 1;
        window_t handle = window_make_handle(window_slot_index(slot), slot);
        if (width > 0 && height > 0)
            slot->gl_render(handle, client_x, client_y, width, height);
    }
    window_irq_restore(flags);
}

void window_set_gl_render_callback(window_t window, window_gl_render_fn callback)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (!slot) { window_irq_restore(flags); return; }
    slot->gl_render = callback;
    slot->wm_window->render_gl_content = callback ? window_gl_render_dispatch : 0;
    desktop_invalidate_rect(slot->wm_window->x, slot->wm_window->y,
                            slot->wm_window->w, slot->wm_window->h);
    window_irq_restore(flags);
}

void window_invalidate(window_t window)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    Window_t *w;
    if (!slot) { window_irq_restore(flags); return; }
    w = slot->wm_window;
    desktop_invalidate_rect(w->x + 1, w->y + WINDOW_TITLEBAR_HEIGHT,
                            w->w - 2, w->h - WINDOW_TITLEBAR_HEIGHT - 1);
    window_irq_restore(flags);
}

static int window_client_origin(window_t window, int *x, int *y)
{
    uint32_t flags = window_irq_save();
    window_api_slot_t *slot = window_slot(window);
    if (!slot) { window_irq_restore(flags); return 0; }
    *x = slot->wm_window->x + 1;
    *y = slot->wm_window->y + WINDOW_TITLEBAR_HEIGHT;
    window_irq_restore(flags);
    return 1;
}

void window_draw_clear(window_t window, unsigned int color)
{
    int x, y, w, h;
    if (!window_client_origin(window, &x, &y)) return;
    window_get_client_size(window, &w, &h);
    gfx_draw_rect(x, y, w, h, color);
}

void window_draw_pixel(window_t window, int x, int y, unsigned int color)
{
    int ox, oy;
    if (window_client_origin(window, &ox, &oy)) gfx_draw_pixel(ox + x, oy + y, color);
}

void window_draw_line(window_t window, int x0, int y0, int x1, int y1, unsigned int color)
{
    int ox, oy;
    if (window_client_origin(window, &ox, &oy)) gfx_draw_line(ox + x0, oy + y0, ox + x1, oy + y1, color);
}

void window_draw_rect(window_t window, int x, int y, int width, int height, unsigned int color)
{
    int ox, oy;
    if (window_client_origin(window, &ox, &oy)) gfx_draw_rect(ox + x, oy + y, width, height, color);
}

void window_draw_rect_outline(window_t window, int x, int y, int width, int height, unsigned int color)
{
    int ox, oy;
    if (window_client_origin(window, &ox, &oy)) gfx_draw_rect_outline(ox + x, oy + y, width, height, color);
}

void window_draw_circle(window_t window, int cx, int cy, int radius, unsigned int color)
{
    int ox, oy;
    if (window_client_origin(window, &ox, &oy)) gfx_draw_circle(ox + cx, oy + cy, radius, color);
}

void window_draw_text(window_t window, int x, int y, const char *text, unsigned int color)
{
    int ox, oy;
    if (window_client_origin(window, &ox, &oy)) gfx_draw_text(ox + x, oy + y, text, color);
}
