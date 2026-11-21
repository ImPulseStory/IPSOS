#include "ipscc.h"
#include <stdint.h>

#define IPSCC_MAX_SOURCE 5120
#define IPSCC_MAX_PATH   255

static char source_buffer[IPSCC_MAX_SOURCE + 1];
static const char *parser_ptr;

static uint8_t code_buffer[1024];
static uint32_t code_size = 0;
static uint8_t data_buffer[1024];
static uint32_t data_size = 0;

/* -------------------------------------------------
   Parser helpers
   ------------------------------------------------- */

static void skip_spaces(void) {
    while (*parser_ptr == ' ' || *parser_ptr == '\t' || *parser_ptr == '\n' || *parser_ptr == '\r') {
        parser_ptr++;
    }
}

static int is_identifier_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static int expect_word(const char *word) {
    skip_spaces();
    const char *p = word;
    const char *s = parser_ptr;
    while (*p && *s == *p) { p++; s++; }
    if (*p != '\0' || is_identifier_char(*s)) return -1;
    parser_ptr = s;
    return 0;
}

static int expect_char(char c) {
    skip_spaces();
    if (*parser_ptr != c) return -1;
    parser_ptr++;
    return 0;
}

/* -------------------------------------------------
   Parse integer
   ------------------------------------------------- */

static int parse_number(uint32_t *result) {
    uint32_t value = 0;
    int digits = 0;
    skip_spaces();
    while (*parser_ptr >= '0' && *parser_ptr <= '9') {
        uint32_t digit = (uint32_t)(*parser_ptr - '0');
        if (value > (2147483647u - digit) / 10u) return -1;
        value = value * 10u + digit;
        parser_ptr++;
        digits++;
    }
    if (!digits) return -1;
    *result = value;
    return 0;
}

/* -------------------------------------------------
   Parse expressions
   ------------------------------------------------- */

static int parse_term(uint32_t *value) {
    uint32_t left, right;
    if (parse_number(&left) != 0) return -1;
    while (1) {
        skip_spaces();
        if (*parser_ptr == '*') {
            parser_ptr++;
            if (parse_number(&right) != 0) return -1;
            left *= right;
        } else if (*parser_ptr == '/') {
            parser_ptr++;
            if (parse_number(&right) != 0) return -1;
            if (right == 0) return -1;
            left /= right;
        } else break;
    }
    *value = left;
    return 0;
}

static int parse_expression(uint32_t *value) {
    uint32_t left, right;
    if (parse_term(&left) != 0) return -1;
    while (1) {
        skip_spaces();
        if (*parser_ptr == '+') {
            parser_ptr++;
            if (parse_term(&right) != 0) return -1;
            left += right;
        } else if (*parser_ptr == '-') {
            parser_ptr++;
            if (parse_term(&right) != 0) return -1;
            left -= right;
        } else break;
    }
    *value = left;
    return 0;
}

/* -------------------------------------------------
   Parse string
   ------------------------------------------------- */

static int parse_string(char *out_buffer, int max_len) {
    skip_spaces();
    if (*parser_ptr != '"') return -1;
    parser_ptr++; 
    int i = 0;
    while (*parser_ptr != '"' && *parser_ptr != '\0') {
        if (i < max_len - 1) out_buffer[i++] = *parser_ptr;
        parser_ptr++;
    }
    if (*parser_ptr != '"') return -1;
    parser_ptr++; 
    out_buffer[i] = '\0';
    return 0;
}

/* -------------------------------------------------
   Add string to data buffer
   ------------------------------------------------- */

static uint32_t add_string(const char *str) {
    uint32_t offset = data_size;
    int i = 0;
    while (str[i] != '\0') {
        if (data_size < sizeof(data_buffer)) {
            data_buffer[data_size++] = str[i++];
        } else return 0xFFFFFFFF; // Ошибка
    }
    if (data_size < sizeof(data_buffer)) {
        data_buffer[data_size++] = '\0';
    }
    return offset;
}

/* -------------------------------------------------
   Generate CALL to term_print
   ------------------------------------------------- */

static void generate_call_term_print(uint32_t str_offset) {
    code_buffer[code_size++] = 0x68; 
    code_buffer[code_size++] = (str_offset >> 0) & 0xFF;
    code_buffer[code_size++] = (str_offset >> 8) & 0xFF;
    code_buffer[code_size++] = (str_offset >> 16) & 0xFF;
    code_buffer[code_size++] = (str_offset >> 24) & 0xFF;

    code_buffer[code_size++] = 0xE8; 
    code_buffer[code_size++] = 0x00;
    code_buffer[code_size++] = 0x00;
    code_buffer[code_size++] = 0x00;
    code_buffer[code_size++] = 0x00;
    
    code_buffer[code_size++] = 0x83;
    code_buffer[code_size++] = 0xC4;
    code_buffer[code_size++] = 0x04;
}

/* -------------------------------------------------
   Parse program
   ------------------------------------------------- */

static int parse_program(uint32_t* output) {
    if (expect_word("int") != 0) return -1;
    if (expect_word("main") != 0) return -1;
    if (expect_char('(') != 0) return -1;
    if (expect_char(')') != 0) return -1;
    if (expect_char('{') != 0) return -1;

    int has_return = 0;

    while (*parser_ptr != '}') {
        skip_spaces();
        if (*parser_ptr == '}') break;

        if (expect_word("term_print") == 0) {
            char str_buf[256];
            if (expect_char('(') != 0) return -1;
            if (parse_string(str_buf, sizeof(str_buf)) != 0) return -1;
            if (expect_char(')') != 0) return -1;
            if (expect_char(';') != 0) return -1;
            
            uint32_t str_off = add_string(str_buf);
            if (str_off == 0xFFFFFFFF) return -1;
            generate_call_term_print(str_off);
        } 
        else if (expect_word("return") == 0) {
            if (parse_expression(output) != 0) return -1;
            if (expect_char(';') != 0) return -1;
            has_return = 1;
            break;
        }
        else {
            return -1; 
        }
    }

    if (expect_char('}') != 0) return -1;
    skip_spaces();
    if (*parser_ptr != '\0') return -1;

    if (!has_return) return -1;
    return 0;
}

/* -------------------------------------------------
   Generate i386 machine code (RET)
   ------------------------------------------------- */

static uint32_t generate_return_code(uint32_t return_value, uint8_t *out) {
    out[0] = 0xB8;
    out[1] = (uint8_t)return_value;
    out[2] = (uint8_t)(return_value >> 8);
    out[3] = (uint8_t)(return_value >> 16);
    out[4] = (uint8_t)(return_value >> 24);
    out[5] = 0xC3;
    return 6;
}

/* -------------------------------------------------
   Command argument parser
   ------------------------------------------------- */

static int get_argument(char **cursor, char **argument) {
    while (**cursor == ' ' || **cursor == '\t') (*cursor)++;
    if (**cursor == '\0') return 0;
    *argument = *cursor;
    while (**cursor != '\0' && **cursor != ' ' && **cursor != '\t') (*cursor)++;
    if (**cursor != '\0') { **cursor = '\0'; (*cursor)++; }
    return 1;
}

/* -------------------------------------------------
   Main IPSCC command
   ------------------------------------------------- */

void cmd_ipscc(const char *args) {
    char *cursor;
    char *input_arg = 0;
    char *output_arg = 0;
    char *extra_arg = 0;

    char input_path[512];
    char output_path[512];

    uint32_t source_size = 0;
    uint32_t return_value = 0;

    if (!args) {
        term_print("Usage: ipscc <input.c> <output.bin>\n");
        return;
    }

    cursor = (char *)args;
    if (!get_argument(&cursor, &input_arg) || !get_argument(&cursor, &output_arg) || get_argument(&cursor, &extra_arg)) {
        term_print("Usage: ipscc <input.c> <output.bin>\n");
        return;
    }

    if (input_arg[0] == '\0' || output_arg[0] == '\0') {
        term_print("Invalid path\n");
        return;
    }

    build_path(input_path, input_arg);
    build_path(output_path, output_arg);

    term_print("IPSCC: reading source...\n");
    if (ips_read_file(input_path, source_buffer, IPSCC_MAX_SOURCE, &source_size) != 0) {
        term_print("IPSCC: cannot read source file\n");
        return;
    }
    source_buffer[source_size] = '\0';

    term_print("IPSCC: parsing...\n");
    parser_ptr = source_buffer;

    code_size = 0;
    data_size = 0;

    if (parse_program(&return_value) != 0) {
        term_print("IPSCC: syntax error\n");
        return;
    }

    term_print("IPSCC: generating i386 code...\n");

    code_size += generate_return_code(return_value, code_buffer + code_size);

    uint8_t final_bin[2048];
    uint32_t final_size = 0;

    uint32_t* header = (uint32_t*)final_bin;
    header[0] = code_size;
    header[1] = data_size;
    final_size = 8;

    for (uint32_t i = 0; i < code_size; i++) {
        final_bin[final_size++] = code_buffer[i];
    }

    for (uint32_t i = 0; i < data_size; i++) {
        final_bin[final_size++] = data_buffer[i];
    }

    if (ips_resolve_path(output_path) < 0) {
        if (ips_create_file(output_path) != 0) {
            term_print("IPSCC: cannot create output file\n");
            return;
        }
    }

    int written = ips_write_file(output_path, final_bin, final_size);
    if (written != (int)final_size) {
        term_print("IPSCC: cannot write output file\n");
        return;
    }

    term_print("IPSCC: compilation successful!\n");
    term_print("Output: ");
    term_print(output_path);
    term_print("\n");
}