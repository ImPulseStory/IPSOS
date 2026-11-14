#include "../include/pic.h"
#include "../include/io.h"
#include <stdint.h>


void pic_remap() {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT);
    outb(PIC2_CMD, ICW1_INIT);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_unmask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA; // 0x21
    } else {
        port = PIC2_DATA; // 0xA1
        irq -= 8;
        
        uint8_t master = inb(PIC1_DATA);
        master = master & ~(1 << 2); 
        outb(PIC1_DATA, master);
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);
}