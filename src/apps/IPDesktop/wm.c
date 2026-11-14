#include "desktop.h"

static Window_t windows[MAX_WINDOWS];
static int window_count = 0;

void wm_draw_all() {
    for (int i = 0; i < window_count; i++) {
        wm_draw(&windows[i]);
    }
}

void wm_focus(Window_t* window) {

}

void wm_close(Window_t* window) {
}

Window_t* wm_create(int x, int y, int w, int h, const char* title, int visible) {
    Window_t* window = &windows[window_count];
    window->x = x;
    window->y = y;
    window->w = w;
    window->h = h;
    window->title = title;
    window->z = 16;
    window->visible = visible;
    
    if (visible == 1) {
        wm_focus(window);
    }

    window_count++;
    return window;
}

void wm_draw(Window_t* window) {
    draw_rect(window->x, window->y, window->w, window->h, 0x101010); 

    draw_rect(window->x, window->y, window->w, 1, 0xFFFFFF); 
    draw_rect(window->x, window->y + window->h - 1, window->w, 1, 0xFFFFFF); 
    draw_rect(window->x, window->y, 1, window->h, 0xFFFFFF); 
    draw_rect(window->x + window->w - 1, window->y, 1, window->h, 0xFFFFFF); 
}

void wm_resize(Window_t* window, int w, int h)  {

}

void wm_set_pos(Window_t* window, int x, int y) {

}

void wm_unvisible(Window_t* window) {

}