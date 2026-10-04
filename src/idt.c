#include "boot.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_load(uint32_t);
extern void isr_stub();
extern void irq_stub();
extern void isr6();
extern void irq1_stub();

void idt_set_gate(int n, uint32_t handler) {
    idt[n].base_low  = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].selector  = 0x08;
    idt[n].always0   = 0;
    idt[n].flags     = 0x8E;
}

void idt_init() {
    idtp.limit = sizeof(struct idt_entry) * 256 - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_stub);
    }
    for (int i = 32; i < 48; i++) {
        idt_set_gate(i, (uint32_t)irq_stub);
    }

    idt_set_gate(0x21, (uint32_t)irq1_stub);
    idt_set_gate(6, (uint32_t)isr6);

    idt_load((uint32_t)&idtp);
}