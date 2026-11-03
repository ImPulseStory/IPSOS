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

    draw_rect(50, 50, 600, 400, 0x101010); 

    draw_rect(50, 50, 600, 1, 0xFFFFFF); 
    draw_rect(50, 50 + 400 - 1, 600, 1, 0xFFFFFF); 
    draw_rect(50, 50, 1, 400, 0xFFFFFF); 
    draw_rect(50 + 600 - 1, 50, 1, 400, 0xFFFFFF); 

    draw_char(350 - 8*4, 34, 'T', 0x1F1F1F);
    draw_char(350 - 8*3, 34, 'E', 0x1F1F1F);
    draw_char(350 - 8*2, 34, 'R', 0x1F1F1F);
    draw_char(350 - 8*1, 34, 'M', 0x1F1F1F);
    draw_char(350 + 8*0, 34, 'I', 0x1F1F1F);
    draw_char(350 + 8*1, 34, 'N', 0x1F1F1F);
    draw_char(350 + 8*2, 34, 'A', 0x1F1F1F);
    draw_char(350 + 8*3, 34, 'L', 0x1F1F1F);

    __asm__ volatile ("sti"); 

    term_run();

    while(1);
}
