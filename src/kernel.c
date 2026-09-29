#include "kernel.h"

void kernel_main() {
    volatile char *vga = (volatile char *) 0xB8000;
    vga[0] = 'G';
    vga[1] = 0x0F;
    while (1) {}
}