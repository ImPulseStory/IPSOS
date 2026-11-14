#include "terminal.h"
#include <stdint.h>

extern void run(const char* file);

static char cmd_buf[256];
static int cmd_len = 0;

int win_x = 0;
int win_y = 0;
int win_w = 1020;
int win_h = 760;

int term_x = 0;
int term_y = 0;

int old_mouse_x = -1;
int old_mouse_y = -1;
uint32_t saved_bg[8 * 8];

void term_clear() {
    gfx_draw_rect(win_x+1, win_y+1, win_w-2, win_h-2, 0x101010);
    term_x = 0;
    term_y = 0;
}

void term_print_char(char c) {
    if (c == '\b') {
        gfx_draw_rect(win_x + term_x, win_y + term_y, 8, 16, 0x101010);
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

    gfx_print_char(win_x + term_x, win_y + term_y, c, 0xFFFFFF);

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
        char* buf = 0;
        if (list(buf, args) == -1) {
            term_print("empty \n");
        } else {
            term_print(buf);
            term_print_char('\n');
        }


    }
    if (strcmp(command, "cat") == 0) {
        if (args != 0) {
            uint8_t buf[512];
            uint32_t size = 0;
            if (read_file(args, (char*)buf) == 0) {
                buf[size] = '\0';
                term_print((char*)buf);
                term_print("\n");
            } else {
                term_print("ERROR");
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

            if (write_to_file(filename, (char*)text) == 0) {
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

        if (deleteEx(args) == 0) {
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

        if (create_file(args) == 0) {
           term_print("Success\n");
        } else {
           term_print("Error: Unable to create file\n");
        }
    }
    if (strcmp(command, "mkdir") == 0) {
        if (args == 0) {
            term_print("Usage: mkdir <name>\n");
            return;
        }
        if (mkdir(args) == 0) {
            term_print("Succes\n");
        }
    }
}

void draw_cursor(int x, int y) {
    gfx_draw_rect(x, y, 10, 10, 0xFF0000);
}

void term_print_int(int n) {
    char buf[512];
    int i = 0;
    if (n == 0) { term_print_char('0');  return;}
    if (n < 0) { term_print_char('-'); n = -n; }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    } while (i > 0) { term_print_char(buf[--i]); }
}

void update_cursor() {
    if (old_mouse_x != -1) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                gfx_draw_pixel(old_mouse_x + x, old_mouse_y + y, saved_bg[y * 8 + x]);
            }
        }
    }

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            saved_bg[y * 8 + x] = gfx_get_pixel(get_mouse_x() + x, get_mouse_y() + y);
        }
    }

    draw_cursor(get_mouse_x(), get_mouse_y());

    old_mouse_x = mouse_x();
    old_mouse_y = mouse_y();
}


void term_run() {
    char cmd_buf[256];
    int cmd_len = 0;
    
    term_print("IPSOS@user: ");
    
    while(1) {
        int mouse_movedd = mouse_moved();
        int key = readKey(); 
        if (key != 0) {
            if (key == '\n') {
                cmd_buf[cmd_len] = '\0'; 
                term_print("\n");        
                
                term_execute(cmd_buf);   
                
                cmd_len = 0;             
                term_print("IPSOS@user: "); 
            } 
            else if (key == '\b') {
                if (cmd_len > 0) {
                    cmd_len--;
                    term_print_char('\b'); 
                }
            }
            else if (key >= 32 && key <= 126) {
                if (cmd_len < 255) {
                    cmd_buf[cmd_len] = (char)key;
                    cmd_len++;
                    term_print_char((char)key); 
                }
            }
        }

        if (mouse_movedd) {
            //update_cursor();
            mouse_movedd = 0;
        }
    }
}

void term_update() {
    term_print("IPSOS@user: ");
    int mouse_movedd = mouse_moved();
    int key = readKey(); 
    if (key != 0) {
        if (key == '\n') {
            cmd_buf[cmd_len] = '\0'; 
            term_print("\n");        
            
            term_execute(cmd_buf);   
                
            cmd_len = 0;             
            term_print("IPSOS@user: "); 
         } 
        else if (key == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                term_print_char('\b'); 
            }
        }
        else if (key >= 32 && key <= 126) {
            if (cmd_len < 255) {
                cmd_buf[cmd_len] = (char)key;
                cmd_len++;
                term_print_char((char)key); 
            }
        }
    }

    if (mouse_movedd) {
        //update_cursor();
        mouse_movedd = 0;
    }
}