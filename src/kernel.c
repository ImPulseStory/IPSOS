#include "boot.h"
#include <stdbool.h>
#include <stdint.h>

uint8_t buf[512];

void execute(const char *cmd) {
    char* command;
    char* args;

    parse_command((char*)cmd, &command, &args);

    if (strcmp(command, "help") == 0) {
        print("Commands aviable: \n");
        print("help - shows all comands\n");
        print("about - shows info about OS\n");
        print("clear - clear the screen\n");
    }
    if (strcmp(command, "about") == 0) {
        print("OS - IPSOS, hello everyone! \n");
    }
    if (strcmp(command, "clear") == 0) {
        clear();
    }
    if (strcmp(command, "run") == 0) {
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
    if (strcmp(command, "ls") == 0) {
        sfs_list();
    }
    if (strcmp(command, "cat") == 0) {
        if (args != 0) {
            uint8_t buf[512];
            uint32_t size;
            if (sfs_read(args, buf, &size) == 0) {
                buf[size] = '\0';
                print((char*)buf);
                print("\n");
            }
        } else {
            print("Usage: cat <filename>\n");
        }
    }
    if (strcmp(command, "write") == 0) {
        if (args == 0) {
            print("Usage: write <file> <text>\n");
            return;
        }

        char* filename = args;
        char* text = 0;

        int i = 0;
        while (args[i] != '\0') {
            if (args[i] == ' ') {
                args[i] = '\0';

                text = &args[i + 1];
                break;
            }
            i++;
        }

        if (filename != 0 && text != 0) {
            uint32_t len = 0;
            while (text[len] != '\0') {
                len++;
            }

            if (sfs_create(filename, (uint8_t*)text, len) == 0) {
                print("Succes writen to ");
                print(filename);
                print("\n");
            } else {
                print("Error: Could not write file\n");
            }
        } else {
            print("Usage: write <file> <text>\n");
        }
    }
    if (strcmp(command, "rm") == 0) {
        if (args == 0) {
            print("Usage: del <file>\n");
            return;
        }

        if (sfs_delete(args) == 0) {
            print("Succes delete file");
            print(args);
            print("\n");
        } else {
            print("Unable to delete file\n");
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