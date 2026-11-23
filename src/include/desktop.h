#ifndef DESKTOP_H
#define DESKTOP_H

#include "ipsos.h"
#include "task.h"
#include "mouse.h"

#define MAX_WINDOWS 16

typedef struct Window_t Window_t;
typedef void (*WindowRenderFn)(Window_t *window);
typedef void (*WindowGLRenderFn)(Window_t *window);

struct Window_t {
    int x, y, w, h;
    const char *title;
    int z;
    int visible;
    int closed;
    WindowRenderFn render_content;
    WindowGLRenderFn render_gl_content;
};

void desktop_init(void);
void desktop_run(void);
void desktop_update(void *arg);
void desktop_invalidate(void);
void desktop_invalidate_rect(int x, int y, int width, int height);
void wm_invalidate_all(void);
void wm_invalidate_rect(int x, int y, int width, int height);
void draw_cursor(void *arg);

void wm_draw_all(void);
void wm_render_gl_windows(void);
void wm_handle_mouse(int x, int y, int buttons);
void wm_focus(Window_t *window);
void wm_close(Window_t *window);
void wm_hide(Window_t *window);
void wm_destroy(Window_t *window);
void wm_show(Window_t *window);
void wm_set_title(Window_t *window, const char *title);
Window_t *wm_create(int x, int y, int w, int h, const char *title, int visible);
void wm_draw(Window_t *window);
void wm_set_renderer(Window_t *window, WindowRenderFn renderer);
void wm_resize(Window_t *window, int w, int h);
void wm_set_pos(Window_t *window, int x, int y);
void wm_unvisible(Window_t *window);

#endif
