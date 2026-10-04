#ifndef BOOT_H
#include <stdint.h>

#define BOOT_H

// --------------- SHELL --------------- //
#define WHITE_ON_BLACK 0x0F

void print_hex(uint8_t value);
void scroll();
void print(const char *c);
void print_char(char c);
void clear();
void printf(const char* fmt, ...);
int strcmp(const char* a, const char* b);

// --------------- IO --------------- //
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}


// --------------- KERNEL --------------- //у
#define VGA_ADDRESS (volatile unsigned short *) 0xB8000

void execute(const char *cmd);
void kernel_main();

// --------------- ATA --------------- //
#define DATA_REGISTER  0x1F0
#define ERROR_REGISTER 0x1F1
#define SECTOR_COUNT   0x1F2
#define LBA_LOW        0x1F3
#define LBA_MID        0x1F4
#define LBA_HIGH       0x1F5
#define DRIVE_HEAD     0x1F6
#define STATUS         0x1F7
#define COMMAND        0x1F7

#define STATUS_BSY     0x80
#define STATUS_DRQ     0x08
#define STATUS_ERR     0x01

int ata_init(void);
void read_sector(unsigned int target_address, unsigned int LBA, unsigned char sector_count);
void ata_write_sector(uint32_t lba, uint8_t* buffer);
void ata_identify(uint16_t* buffer);

// --------------- IDT --------------- //
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void idt_init();
void idt_set_gate(int n, uint32_t handler);

// --------------- KEYBOARD DRIVER --------------- //
#define KEYBOARD_DATA_PORT 0x60
#define PIC1_CMD           0x20

void keyboard_init();
void keyboard_handler();
char* input();

// --------------- PIC --------------- //
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x11
#define ICW4_8086 0x01

void pic_remap();
void pic_unmask(uint8_t irq);

#endif