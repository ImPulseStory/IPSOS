#include "../include/mouse.h"
#include "../include/io.h"
#include "../include/gfx.h"
#include <stdint.h>

volatile int mouse_xx = 512;
volatile int mouse_yy = 384;
volatile int mouse_movedd = 0;
volatile uint8_t mouse_button_state = 0;

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];


static void ps2_wait_write() {
    for (int i = 0; i < 100000; i++) {
        if ((inb(0x64) & 0x02) == 0) return;
    }
}

static void ps2_wait_read() {
    for (int i = 0; i < 100000; i++) {
        if (inb(0x64) & 0x01) return;
    }
}

static int mouse_send_cmd(uint8_t cmd) {
    ps2_wait_write();
    outb(0x64, 0xD4); 

    ps2_wait_write();
    outb(0x60, cmd);  
    
    ps2_wait_read();
    uint8_t ack = inb(0x60);
    return (ack == 0xFA) ? 1 : 0; 
}


void mouse_init() {
    mouse_button_state = 0;

    ps2_wait_write();
    outb(0x64, 0xA8);

    ps2_wait_write();
    outb(0x64, 0x20); 
    ps2_wait_read();
    uint8_t conf = inb(0x60);

    conf = conf | 0x02;  
    conf = conf & ~0x20; 

    ps2_wait_write();
    outb(0x64, 0x60); 
    ps2_wait_write();
    outb(0x60, conf);

    mouse_send_cmd(0xFF); 
    ps2_wait_read(); inb(0x60); 
    ps2_wait_read(); inb(0x60); 
    ps2_wait_read(); inb(0x60); 

    mouse_send_cmd(0xF4);

}


void mouse_handler() {
    uint8_t status = inb(0x64);

    if (!(status & 0x20)) return;

    if (!(status & 0x01)) return;

    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            mouse_byte[0] = data;
            if (data & 0xC0) { mouse_cycle = 0; return; }
            mouse_cycle++;
            break;
        case 1:
            mouse_byte[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = data;
            mouse_cycle = 0;

            uint8_t flags = (uint8_t)mouse_byte[0];
            int dx = mouse_byte[1];
            int dy = mouse_byte[2];

            if (flags & 0x10) dx |= 0xFFFFFF00;
            if (flags & 0x20) dy |= 0xFFFFFF00;

            mouse_xx += dx;
            mouse_yy -= dy;
            mouse_button_state = flags & 0x07u;

            int screen_w = (int)gfx_width();
            int screen_h = (int)gfx_height();
            if (screen_w <= 0) screen_w = 1024;
            if (screen_h <= 0) screen_h = 768;

            if (mouse_xx < 0) mouse_xx = 0;
            if (mouse_yy < 0) mouse_yy = 0;
            if (mouse_xx >= screen_w) mouse_xx = screen_w - 1;
            if (mouse_yy >= screen_h) mouse_yy = screen_h - 1;

            mouse_movedd = 1; 
            break;
    }
}

int get_mouse_x() { return mouse_xx; }
int get_mouse_y() { return mouse_yy; }
int get_mouse_moved() { return mouse_movedd; }
int get_mouse_buttons() { return mouse_button_state; }