#include "terminal.h"

extern void run(const char* file);

int win_x = 50;
int win_y = 50;
int win_w = 600;
int win_h = 400;

int term_x = 0;
int term_y = 0;

void term_clear() {
    draw_rect(win_x+1, win_y+1, win_w-2, win_h-2, 0x101010);
    term_x = 0;
    term_y = 0;
}

void term_print_char(char c) {
    if (c == '\b') {
        draw_rect(win_x + term_x, win_y + term_y, 8, 16, 0x101010);
        term_x -= 8;
    }
    if (c == '\n') {
        term_x = 0;
        term_y += 16;
        if (term_y >= win_h) {
            term_y = win_h - 16;
        }
        return;
    }

    draw_char(win_x + term_x, win_y + term_y, c, 0xFFFFFF);

    term_x += 8;
    if (term_x >= win_w) {
        term_x = 0;
        term_y += 16;
    }
}

void term_print(const char* str) {
    while (*str) {
        term_print_char(*str);
        str++;
    }
}

void term_parse_command(char* cmd, char** command, char** args) {
    *command = cmd;
    *args = 0;

    int i = 0;
    while (cmd[i] != '\0') {
        if (cmd[i] == ' ') {
            cmd[i] = '\0';
            *args = &cmd[i + 1];
            break;
        }
        i++;
    }
}

void term_execute(const char *cmd) {
    uint8_t buf[512];
    char* command;
    char* args;

    term_parse_command((char*)cmd, &command, &args);

    if (strcmp(command, "help") == 0) {
       term_print("Commands aviable: \n");
       term_print("help - shows all comands\n");
       term_print("about - shows info about OS\n");
       term_print("clear - clear the screen\n");
    }
    if (strcmp(command, "about") == 0) {
       term_print("OS - IPSOS, hello everyone! \n");
    }
    if (strcmp(command, "clear") == 0) {
        term_clear();
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

                case 0x30: { //term_printC (print an char)
                    char value = stack[cursor - 1];
                   term_print_char(value);
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
                   term_print("FATAL RPOGRAM ERROR \n");
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
               term_print((char*)buf);
               term_print("\n");
            }
        } else {
           term_print("Usage: cat <filename>\n");
        }
    }
    if (strcmp(command, "write") == 0) {
        if (args == 0) {
           term_print("Usage: write <file> <text>\n");
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
               term_print("Succes writen to ");
               term_print(filename);
               term_print("\n");
            } else {
               term_print("Error: Could not write file\n");
            }
        } else {
           term_print("Usage: write <file> <text>\n");
        }
    }
    if (strcmp(command, "rm") == 0) {
        if (args == 0) {
           term_print("Usage: del <file>\n");
            return;
        }

        if (sfs_delete(args) == 0) {
           term_print("Succes delete file");
           term_print(args);
           term_print("\n");
        } else {
           term_print("Unable to delete file\n");
        }
    }
    if (strcmp(command, "edit") == 0) {
        if (args == 0) {
           term_print("Usage: edit <file>\n");
            return;
        }

        run(args);
    }
    if (strcmp(command, "touch") == 0) {
        if (args == 0) {
           term_print("Usage: touch <filename>\n");
            return;
        }

        if (sfs_create(args, 0, 0) == 0) {
           term_print("Success\n");
        } else {
           term_print("Error: Unable to create file\n");
        }
    }
}

void term_run() {
    while(1) {
        term_print("IPSOS@user: ");
        char* cmd = input();
        term_execute(cmd);
    }
}