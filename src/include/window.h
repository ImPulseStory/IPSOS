#ifndef IPSOS_WINDOW_H
#define IPSOS_WINDOW_H

/* Public TCC window API. Handles are generation-tagged; WM internals stay private. */
typedef unsigned int window_t;

typedef struct window_config {
    const char *title;
    int x, y;
    int width, height;
    int visible;
} window_config_t;

typedef void (*window_render_fn)(window_t window, int client_width, int client_height);
/* Called after the 2D desktop has been presented. Coordinates are screen-space
   client bounds; viewport Y must be converted to OpenGL bottom-left origin. */
typedef void (*window_gl_render_fn)(window_t window, int client_x, int client_y,
                                    int client_width, int client_height);

window_t window_create(const window_config_t *config);
void window_destroy(window_t window);
void window_show(window_t window);
void window_hide(window_t window);
int window_is_open(window_t window);
void window_set_title(window_t window, const char *title);
void window_set_position(window_t window, int x, int y);
void window_set_size(window_t window, int width, int height);
void window_get_client_size(window_t window, int *width, int *height);
void window_set_render_callback(window_t window, window_render_fn callback);
void window_set_gl_render_callback(window_t window, window_gl_render_fn callback);
void window_invalidate(window_t window);

/* Coordinates are relative to the top-left corner of the client area. */
void window_draw_clear(window_t window, unsigned int color);
void window_draw_pixel(window_t window, int x, int y, unsigned int color);
void window_draw_line(window_t window, int x0, int y0, int x1, int y1, unsigned int color);
void window_draw_rect(window_t window, int x, int y, int width, int height, unsigned int color);
void window_draw_rect_outline(window_t window, int x, int y, int width, int height, unsigned int color);
void window_draw_circle(window_t window, int cx, int cy, int radius, unsigned int color);
void window_draw_text(window_t window, int x, int y, const char *text, unsigned int color);

#endif
