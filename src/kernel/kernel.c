#include "../include/kernel.h"
#include "../include/libc.h"
#include <stdbool.h>
#include <stdint.h>

void kernel_main() {
    idt_init();
    pic_remap();
    pic_unmask(0);
    pic_unmask(1);
    keyboard_init();

    clear();
    ata_init();

    sfs_init();
    print("Hello from IPSOS! \n");

    outb(0x70, inb(0x70) | 0x80);
    __asm__ volatile ("sti");

    while(1) {
        print("IPSOS@user: ");
        char* cmd = input();
        execute(cmd);
    }
}