#include "../include/pit.h"
#include "../include/io.h"

void pit_init(uint32_t frequency)
{
    uint32_t divisor;

    if (!frequency)
        return;

    divisor = 1193182u / frequency;
    if (!divisor)
        divisor = 1u;
    if (divisor > 65535u)
        divisor = 65535u;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFFu));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFFu));
}
