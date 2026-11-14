#ifndef DESKTOP_H
#define DESKTOP_H

#include "../../api/api.h"

#define MAX_WINDOWS 16

// #---------------- DESKTOP -----------# //

void desktop_init();
void desktop_run();
void desktop_update();

// # ---------------- WINDOW MANAGER --------- # //
typedef struct Window_t {
    int x, y, w, h;
    const char* title;
    int z;
    int visible;
} Window_t;

void wm_draw_all();
void wm_focus(Window_t* window);
void wm_close(Window_t* window);
Window_t* wm_create(int x, int y, int w, int h, const char* title, int visible);
void wm_draw(Window_t* window);

void wm_resize(Window_t* window, int w, int h);
void wm_set_pos(Window_t* window, int x, int y);
void wm_unvisible(Window_t* window);

#endif