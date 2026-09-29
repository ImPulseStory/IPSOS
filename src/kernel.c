#define WHITE_ON_BLACK 0x0F

volatile unsigned short *vga = (volatile unsigned short *) 0xB8000;

void print(const char *c) {
    int cursor = 0;
    while (*c != '\0') {
        vga[cursor] = (WHITE_ON_BLACK << 8) | *c;
        cursor++;
        c++;
    }
}

void clear() {
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            vga[y * 80 + x] = (0x0F << 8) | ' ';
        }
    }
}

void kernel_main() {
    clear();
    print("Hello from IPSOS");
    while(1) {
    }
}