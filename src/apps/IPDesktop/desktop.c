#include "desktop.h"

void desktop_run() {
    while(1) {
        desktop_update();
    }
}

void desktop_update() {
    wm_draw_all();
}

void desktop_init() {
    gfx_clear_screen(0x0000FF);
}