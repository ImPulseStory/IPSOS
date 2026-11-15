#include "../include/kernel.h"
#include <stdbool.h>
#include <stdint.h>

void kernel_main() {
    __asm__ volatile ("cli");
    idt_init();
    pic_remap();
    pic_unmask(0);
    pic_unmask(1);
    pic_unmask(2);
    pic_unmask(12);
    keyboard_init();
    mouse_init(); 
    ips_init();

    vbe_init();

    gfx_clear_screen(0x000000);

    __asm__ volatile ("sti"); 

    term_run();
}
