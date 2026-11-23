#include "../include/kernel.h"
#include <stdbool.h>
#include <stdint.h>
#include "../include/task_internal.h"
#include "../include/pit.h"
#include "../include/desktop.h"
#include "../include/virtio_gpu.h"
#include "../include/virgl.h"

extern void ips_seed_examples(void);

static void debug_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
        outb(0xE9u, (uint8_t)hex[(value >> shift) & 0xFu]);
}

void kernel_main() {
    __asm__ volatile ("cli");
    idt_init();
    pic_remap();
    pic_unmask(1);
    pic_unmask(2);
    pic_unmask(12);
    keyboard_init();
    mouse_init();
    task_init();
    pit_init(PIT_FREQUENCY);
    pic_unmask(0);
    ips_init();
    ips_seed_examples();

    if (gfx_init() != 0) {
        const char *message = "IPSOS: VirtIO-GPU initialization failed\n";
        while (*message)
            outb(0xE9u, (uint8_t)*message++);
        {
            static const char hex[] = "0123456789ABCDEF";
            uint32_t error = gfx_driver_error();
            outb(0xE9u, '0');
            outb(0xE9u, 'x');
            for (int shift = 28; shift >= 0; shift -= 4)
                outb(0xE9u, (uint8_t)hex[(error >> shift) & 0xFu]);
            outb(0xE9u, ' ');
            outb(0xE9u, 'c');
            outb(0xE9u, 'm');
            outb(0xE9u, 'd');
            outb(0xE9u, '=');
            outb(0xE9u, '0');
            outb(0xE9u, 'x');
            error = virtio_gpu_last_command();
            for (int shift = 28; shift >= 0; shift -= 4)
                outb(0xE9u, (uint8_t)hex[(error >> shift) & 0xFu]);
            outb(0xE9u, '\n');
        }
        for (;;)
            __asm__ volatile("cli; hlt");
    }
    if (virgl_init() != 0) {
        const char *message = "IPSOS: VirGL 3D context unavailable; 2D renderer remains active\n";
        while (*message) outb(0xE9u, (uint8_t)*message++);
        outb(0xE9u, ' '); outb(0xE9u, 's'); outb(0xE9u, 't');
        outb(0xE9u, 'a'); outb(0xE9u, 'g'); outb(0xE9u, 'e'); outb(0xE9u, '=');
        debug_hex32(virgl_last_error());
        outb(0xE9u, ' '); outb(0xE9u, 's'); outb(0xE9u, 'u');
        outb(0xE9u, 'p'); outb(0xE9u, 'p'); outb(0xE9u, '=');
        debug_hex32((uint32_t)virtio_gpu_virgl_supported());
        outb(0xE9u, ' '); outb(0xE9u, 'g'); outb(0xE9u, 'p');
        outb(0xE9u, 'u'); outb(0xE9u, '_'); outb(0xE9u, 'e');
        outb(0xE9u, 'r'); outb(0xE9u, 'r'); outb(0xE9u, '=');
        debug_hex32(virtio_gpu_last_error());
        outb(0xE9u, ' '); outb(0xE9u, 'c'); outb(0xE9u, 'm');
        outb(0xE9u, 'd'); outb(0xE9u, '=');
        debug_hex32(virtio_gpu_last_command());
        outb(0xE9u, ' '); outb(0xE9u, 'r'); outb(0xE9u, 'l');
        outb(0xE9u, 'e'); outb(0xE9u, 'n'); outb(0xE9u, '=');
        debug_hex32(virtio_gpu_last_response_length());
        outb(0xE9u, '\n');
    }
    gfx_clear(0x000000FFu);
    gfx_present();
    set_up();
    desktop_run();

    __asm__ volatile ("sti");

    term_run();
}
