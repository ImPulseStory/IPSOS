#include "../include/kernel.h"
#include <stdbool.h>
#include <stdint.h>

void kernel_main() {
    idt_init();
    pic_remap();
    pic_unmask(0);
    pic_unmask(1);
    keyboard_init();

    vbe_init();

    clear_screen(0x0000FF);
    draw_char(100, 100, 'h', 0x000000);
    draw_char(110, 100, 'e', 0x000000);
    draw_char(120, 100, 'l', 0x000000);
    draw_char(130, 100, 'l', 0x000000);
    draw_char(140, 100, 'o', 0x000000);
   //flush_screen();

    while (1) {
        __asm__ volatile ("hlt");
    }
}
