#include "terminal.h"
#include "../../tcc/tcc_port.h"
#include "../../include/exec.h"
#include "../../include/gfx.h"
#include "../../include/unistd.h"
#include <stdint.h>

extern void run(const char* file);

static char cmd_buf[256];
static int cmd_len = 0;
static char cwd[256] = "/";

int win_x = 0;
int win_y = 0;
int win_w = 1025;
int win_h = 768;

int term_x = 0;
int term_y = 0;
static Window_t *terminal_window = 0;

static void term_invalidate_client(void)
{
    if (terminal_window)
        desktop_invalidate_rect(terminal_window->x + 1, terminal_window->y + 22,
                                terminal_window->w - 2, terminal_window->h - 23);
    else
        desktop_invalidate();
}

static void term_invalidate_cell(int x, int y)
{
    if (terminal_window)
        desktop_invalidate_rect(terminal_window->x + 1 + x, terminal_window->y + 22 + y, 8, 16);
    else
        desktop_invalidate();
}

#define TERM_MAX_COLS 128
#define TERM_MAX_ROWS 48
#define TERM_BACKGROUND 0x00101010u

typedef struct {
    char character;
    uint32_t color;
} TerminalCell;

static TerminalCell terminal_cells[TERM_MAX_COLS * TERM_MAX_ROWS];
static int terminal_cols;
static int terminal_rows;

static uint32_t terminal_irq_save(void);
static void terminal_irq_restore(uint32_t flags);

static void term_redraw_window(Window_t *window) {
    int cols;
    int rows;
    int origin_x;
    int origin_y;
    int clip_x, clip_y, clip_w, clip_h;
    int first_col, last_col, first_row, last_row;

    if (!window)
        return;

    cols = (window->w - 2) / 8;
    rows = (window->h - 23) / 16;
    if (cols > terminal_cols) cols = terminal_cols;
    if (rows > terminal_rows) rows = terminal_rows;
    if (cols <= 0 || rows <= 0) return;

    origin_x = window->x + 1;
    origin_y = window->y + 22;
    gfx_get_clip(&clip_x, &clip_y, &clip_w, &clip_h);
    gfx_draw_rect(origin_x, origin_y, cols * 8, rows * 16, TERM_BACKGROUND);

    first_col = (clip_x - origin_x) / 8;
    first_row = (clip_y - origin_y) / 16;
    last_col = (clip_x + clip_w - origin_x + 7) / 8;
    last_row = (clip_y + clip_h - origin_y + 15) / 16;
    if (first_col < 0) first_col = 0;
    if (first_row < 0) first_row = 0;
    if (last_col > cols) last_col = cols;
    if (last_row > rows) last_row = rows;

    for (int row = first_row; row < last_row; ++row) {
        for (int col = first_col; col < last_col; ++col) {
            TerminalCell *cell = &terminal_cells[row * terminal_cols + col];
            uint32_t flags = terminal_irq_save();
            char character = cell->character;
            uint32_t color = cell->color;
            terminal_irq_restore(flags);

            if (character && character != ' ')
                gfx_draw_char(origin_x + col * 8, origin_y + row * 16,
                              character, color);
        }
    }
}

static uint32_t terminal_irq_save(void) {
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void terminal_irq_restore(uint32_t flags) {
    if (flags & 0x200u)
        __asm__ volatile("sti" : : : "memory");
}

typedef int (*program_main_t)(void);
int run_binary(const char *path) {
    static uint8_t binary_buffer[BLOCK_SIZE * DIRECT_BLOCKS];
    uint32_t binary_size = 0;

    if (!path) return -1;

    if (read_file(path, binary_buffer, sizeof(binary_buffer), &binary_size) != 0) {
        term_print("RUN: cannot read file\n");
        return -1;
    }
    if (binary_size < 8) {
        term_print("RUN: file too small\n");
        return -1;
    }

    uint32_t code_size = *(uint32_t*)(binary_buffer + 0);
    uint32_t data_size = *(uint32_t*)(binary_buffer + 4);

    uint8_t* code = binary_buffer + 8;
    uint8_t* data = binary_buffer + 8 + code_size;

    for (uint32_t i = 0; i < code_size - 5; i++) {
        if (code[i] == 0xE8) { 
            int32_t rel = (int32_t)((uint32_t)term_print - (uint32_t)(code + i + 5));
            code[i+1] = (rel >> 0) & 0xFF;
            code[i+2] = (rel >> 8) & 0xFF;
            code[i+3] = (rel >> 16) & 0xFF;
            code[i+4] = (rel >> 24) & 0xFF;
        }
    }

    for (uint32_t i = 0; i < code_size - 5; i++) {
        if (code[i] == 0x68) { 
            uint32_t str_offset = code[i+1] | (code[i+2] << 8) | (code[i+3] << 16) | (code[i+4] << 24);
            
            uint32_t str_addr = (uint32_t)data + str_offset; 
            
            code[i+1] = (str_addr >> 0) & 0xFF;
            code[i+2] = (str_addr >> 8) & 0xFF;
            code[i+3] = (str_addr >> 16) & 0xFF;
            code[i+4] = (str_addr >> 24) & 0xFF;
        }
    }

    typedef int (*binary_main_t)(void);
    binary_main_t program = (binary_main_t)(uintptr_t)code;
    
    int result = program();
    
    return result;
}

void build_path(char* out, const char* arg) {
    if (arg[0] == '/') {
        strcpy(out, arg);
    } 
    else {
        strcpy(out, cwd);
        if (out[1] != '\0') { 
            strcat(out, "/");
        }
        strcat(out, arg);
    }
}

static void term_scroll_up(void) {
    uint32_t flags;

    if (terminal_rows <= 0 || terminal_cols <= 0) {
        term_y = 0;
        return;
    }

    flags = terminal_irq_save();
    for (int row = 0; row < terminal_rows - 1; ++row) {
        for (int col = 0; col < terminal_cols; ++col)
            terminal_cells[row * terminal_cols + col] =
                terminal_cells[(row + 1) * terminal_cols + col];
    }

    for (int col = 0; col < terminal_cols; ++col) {
        TerminalCell *cell = &terminal_cells[(terminal_rows - 1) * terminal_cols + col];
        cell->character = ' ';
        cell->color = 0x00FFFFFFu;
    }

    term_y = (terminal_rows - 1) * 16;
    terminal_irq_restore(flags);
}

static int term_new_line(void) {
    term_x = 0;
    term_y += 16;

    if (term_y + 16 > win_h) {
        term_scroll_up();
        return 1;
    }
    return 0;
}

static void term_draw_cell(int x, int y, char c, uint32_t color) {
    int col = x / 8;
    int row = y / 16;

    if (col < 0 || col >= terminal_cols || row < 0 || row >= terminal_rows)
        return;

    uint32_t flags = terminal_irq_save();
    terminal_cells[row * terminal_cols + col].color = color;
    terminal_cells[row * terminal_cols + col].character = c;
    terminal_irq_restore(flags);
}

static void term_clear_cell(int x, int y) {
    int col = x / 8;
    int row = y / 16;

    if (col < 0 || col >= terminal_cols || row < 0 || row >= terminal_rows)
        return;

    uint32_t flags = terminal_irq_save();
    terminal_cells[row * terminal_cols + col].character = ' ';
    terminal_cells[row * terminal_cols + col].color = 0x00FFFFFFu;
    terminal_irq_restore(flags);
}

void term_clear(void) {
    uint32_t flags = terminal_irq_save();

    for (int i = 0; i < terminal_cols * terminal_rows; ++i) {
        terminal_cells[i].character = ' ';
        terminal_cells[i].color = 0x00FFFFFFu;
    }

    term_x = 0;
    term_y = 0;
    terminal_irq_restore(flags);
    term_invalidate_client();
}

void term_print_char(char c) {
    int cell_x, cell_y;
    if (c == '\b') {
        if (term_x >= 8) {
            term_x -= 8;
        } else if (term_y >= 16) {
            term_y -= 16;
            term_x = win_w - 8;
        } else {
            return;
        }
        term_clear_cell(term_x, term_y);
        term_invalidate_cell(term_x, term_y);
        return;
    }

    if (c == '\n') {
        if (term_new_line()) term_invalidate_client();
        return;
    }

    cell_x = term_x;
    cell_y = term_y;
    term_draw_cell(cell_x, cell_y, c, 0x00FFFFFFu);
    term_x += 8;
    if (term_x >= win_w && term_new_line())
        term_invalidate_client();
    else
        term_invalidate_cell(cell_x, cell_y);
}

void term_print_char_cl(char c, uint32_t color) {
    int cell_x, cell_y;
    if (c == '\b') {
        if (term_x >= 8) {
            term_x -= 8;
        } else if (term_y >= 16) {
            term_y -= 16;
            term_x = win_w - 8;
        } else {
            return;
        }
        term_clear_cell(term_x, term_y);
        term_invalidate_cell(term_x, term_y);
        return;
    }

    if (c == '\n') {
        if (term_new_line()) term_invalidate_client();
        return;
    }

    cell_x = term_x;
    cell_y = term_y;
    term_draw_cell(cell_x, cell_y, c, color);
    term_x += 8;
    if (term_x >= win_w && term_new_line())
        term_invalidate_client();
    else
        term_invalidate_cell(cell_x, cell_y);
}

void term_print_cl(const char *str, uint32_t color) {
    while (*str)
        term_print_char_cl(*str++, color);
}

void term_print(const char *str) {
    while (*str)
        term_print_char(*str++);
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

void shell_cd(const char* arg) {
    char new_path[256];

    if (!arg || !*arg)
        return;

    if (strcmp(arg, "..") == 0) {
        strcpy(new_path, cwd);
        if (new_path[1] == '\0')
            return;
        int i = strlen(new_path) - 1;
        while (i > 0 && new_path[i] != '/')
            --i;
        if (i == 0)
            new_path[1] = '\0';
        else
            new_path[i] = '\0';
    } else if (strcmp(arg, ".") == 0) {
        strcpy(new_path, cwd);
    } else {
        build_path(new_path, arg);
    }

    if (chdir(new_path) == 0)
        getcwd(cwd, sizeof(cwd));
    else
        term_print("cd: directory not found\n");
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
       term_print("cd <path> - change working directory\n");
       term_print("ls [path] - show directory");
       term_print("cat <file> - show file");
       term_print("touch <file> - create empty file");
       term_print("run - BETA (interpreteter)");
       term_print("write <file> <contain> - write contain to file");
       term_print("pwd - show absolute working path");
       term_print("echo - echo, just echo\n");
       term_print("tcc <source.c> [-o output] - compile C\n");
       term_print("./program - run executable\n");
    }
    if (strcmp(command, "ipscc") == 0) {
        cmd_ipscc(args);
    }
    if (strcmp(command, "tcc") == 0) {
        if (!args || !*args) {
            term_print("Usage: tcc input.c [-o output] [other.c]\n");
        } else if (ips_tcc_compile(args) < 0) {
            term_print("TCC: failed\n");
        }
    }
    if (strcmp(command, "irun") == 0) {
        char target_path[256];
        build_path(target_path, args);
        int result = run_binary(target_path);
        if (result != 0) {
            term_print_int(result);
            term_print("\n");
        }
    }
    if (strcmp(command, "pwd") == 0) {
        term_print(cwd);
        term_print("\n");
    }
    if (strcmp(command, "echo") == 0) {
        term_print(args);
        term_print("\n");
    }
    if (strcmp(command, "about") == 0) {
       term_print("OS - IPSOS, hello everyone! \n");
    }
    if (strcmp(command, "clear") == 0) {
        term_clear();
    } 
    if (strcmp(command, "cls") == 0) {
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
        char buf[IPSFS_LIST_BUFFER_SIZE];
        char target_path[256];
        size_t arg_len = args ? strlen(args) : 0;
        size_t cwd_len = strlen(cwd);
        size_t prefix_len = (arg_len && args[0] != '/')
            ? cwd_len + (cwd_len > 1 ? 1u : 0u)
            : 0u;
        int result;

        if (prefix_len + arg_len >= sizeof(target_path)) {
            term_print("ls: path too long\n");
        } else {
            if (!arg_len)
                strcpy(target_path, cwd);
            else
                build_path(target_path, args);

            result = list_ex(buf, target_path, sizeof(buf));
            if (result < 0) {
                term_print("ls: cannot read directory\n");
            } else {
                term_print(buf);
            }
        }
    }
    if (strcmp(command, "cat") == 0) {
        char buf[8192];
        uint32_t size = 0;
        char target_path[256];

        build_path(target_path, args);
        if (read_file(target_path, buf, sizeof(buf) - 1, &size) == 0) {
            buf[size] = '\0';
            term_print(buf);
            term_print("\n");
        } else {
            term_print("ERROR\n");
        }
    }
    if (strcmp(command, "cd") == 0) {
        shell_cd(args);
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

            char target_path[256];

            build_path(target_path, args);
            int result = write_to_file(target_path, text);

            if (result >= 0) {
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
        
        char target_path[256];
        build_path(target_path, args);

        if (deleteEx(target_path) == 0) {
        } else {
           term_print("Unable to delete file\n");
        }
    }
    if (strcmp(command, "edit") == 0) {
        if (args == 0) {
           term_print("Usage: edit <file>\n");
            return;
        }
        
        char target_path[256];
        build_path(target_path, args);
        run(target_path);
    }
    if (strcmp(command, "touch") == 0) {
        if (args == 0) {
           term_print("Usage: touch <filename>\n");
            return;
        }

        char target_path[256];

        build_path(target_path, args);
        if (create_file(target_path) == 0) {
        } else {
           term_print("Error: Unable to create file\n");
        }
    }
    if (strcmp(command, "mkdir") == 0) {
        if (args == 0) {
            term_print("Usage: mkdir <name>\n");
            return;
        }
        char target_path[256];

        build_path(target_path, args);
        if (mkdir(target_path) == 0) {
        }
    }
    if (command[0] == '.' && command[1] == '/') {
        char target_path[256];
        build_path(target_path, command + 2);
        int result = exec_elf(target_path, args);
        if (result < 0) {
            term_print("EXEC: failed\n");
        }
    } else if (command[0] == '/') {
        char target_path[256];
        build_path(target_path, command);
        int result = exec_elf(target_path, args);
        if (result < 0) {
            term_print("EXEC: failed\n");
        }
    }
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

void set_up() {
    if (terminal_window)
        return;

    terminal_window = wm_create(100, 100, 900, 600, "TERMINAL", 1);
    if (!terminal_window)
        return;

    win_x = terminal_window->x + 1;
    win_y = terminal_window->y + 22;
    win_w = ((terminal_window->w - 2) / 8) * 8;
    win_h = ((terminal_window->h - 23) / 16) * 16;
    terminal_cols = win_w / 8;
    terminal_rows = win_h / 16;
    if (terminal_cols > TERM_MAX_COLS)
        terminal_cols = TERM_MAX_COLS;
    if (terminal_rows > TERM_MAX_ROWS)
        terminal_rows = TERM_MAX_ROWS;
    wm_set_renderer(terminal_window, term_redraw_window);
    term_clear();
}

void term_run() {
    char cmd_buf[256];

    if (!terminal_window)
        set_up();
    if (!terminal_window)
        return;
    int cmd_len = 0;

    char greeting[64];
    greeting[0] = '\0';
    strcpy(greeting, "IPSOS@");
    strcat(greeting, cwd);
    strcat(greeting, " $ ");
    term_print(greeting);
    
    while(1) {
        int mouse_movedd = mouse_moved();
        int key = readKey(); 
        if (key != 0) {
            if (key == '\n') {
                cmd_buf[cmd_len] = '\0'; 
                term_print("\n");        
                
                term_execute(cmd_buf);
                
                cmd_len = 0;             
                char greeting[64];
                strcpy(greeting, "IPSOS@");
                strcat(greeting, cwd);
                strcat(greeting, " $ ");
                term_print(greeting);
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

void update_terminal() {
    int key = readKey();

    if (key != 0) {
        term_print_char(key);
    }
}