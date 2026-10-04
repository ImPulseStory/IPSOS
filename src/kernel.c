#include "boot.h"
#include <stdbool.h>

uint8_t buf[512];

void execute(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        print("Commands aviable: \n");
        print("help - shows all comands\n");
        print("about - shows info about OS\n");
        print("clear - clear the screen\n");
    }
    if (strcmp(cmd, "about") == 0) {
        print("OS - IPSOS, hello everyone! \n");
    }
    if (strcmp(cmd, "clear") == 0) {
        clear();
    }
    if (strcmp(cmd, "run") == 0) {
        int pc = 0;
        char stack[100];
        bool run = true;
        int cursor = 0;

        while (run) {
            uint8_t opcode = buf[pc++];

            switch (opcode) { // PUSH (push value to stack)
                case 0x01: {
                    stack[cursor] = buf[pc++];
                    cursor++;
                    break;
                }

                case 0x30: { // PRINTC (print an char)
                    char value = stack[cursor - 1];
                    print_char(value);
                    break;
                }

                case 0xFF: { // HLT (stop run programm)
                    run = false;
                    break;
                }

                case 0x00: {
                    break;
                }
                
                default: {
                    print_hex(stack[cursor]);
                    print("FATAL RPOGRAM ERROR \n");
                    run = false;
                    break;
                }
            }
        }
    }
}

void kernel_main() {
    idt_init();
    pic_remap();
    pic_unmask(0);
    pic_unmask(1);
    keyboard_init();

    clear();
    if (ata_init()) {
        print("ATA SUCCESS \n");
    } else {
        print("ATA ERROR");
    }
    print("Hello from IPSOS! \n pidor! \n");

    outb(0x70, inb(0x70) | 0x80);
    __asm__ volatile ("sti");

    read_sector((unsigned int)buf, 100, 1);

    while(1) {
        print("IPSOS@user: ");
        char* cmd = input();
        execute(cmd);
    }
}