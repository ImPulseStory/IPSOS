#include "boot.h"
#include <stdint.h>

static void ata_wait_bsy(void) {
    while (inb(STATUS) & 0x80);
}

static void ata_wait_drq(void) {
    while (!(inb(STATUS) & 0x40));
}

int ata_init(void) {
    ata_wait_bsy();

    outb(DRIVE_HEAD, 0xE0);
    for (int i = 0; i < 4; i++) inb(STATUS);

    outb(STATUS, 0xEC);

    uint8_t status = inb(STATUS);
    if (status == 0) {
        print_char('a');
        return 0;
    }

    ata_wait_bsy();

    if (inb(LBA_MID) != 0 || inb(LBA_HIGH) != 0) {
        print_char('w');
        return 0;
    }

    while (1) {
        status = inb(STATUS);
        if (status & 0x01) { print_char('e'); return 0; }
        if (status & 0x08) break;
    }

    for (int i = 0; i < 256; i++) inw(DATA_REGISTER);

    return 1;
}

void read_sector(unsigned int target_address, unsigned int LBA, unsigned char sector_count)
{
	ata_wait_bsy();
	outb(0x1F6, 0xF0 | ((LBA >> 24) & 0xF));
    for (int i = 0; i < 4; i++) inb(STATUS);
    outb(0x1F2, sector_count);
	outb(0x1F3, (unsigned char) LBA);
	outb(0x1F4, (unsigned char)(LBA >> 8));
	outb(0x1F5, (unsigned char)(LBA >> 16));
	outb(0x1F7,0x20); //Send the read command

	unsigned short *target = (unsigned short*) target_address;

	for (int j =0;j<sector_count;j++)
	{
		ata_wait_bsy();
		ata_wait_drq();
		for(int i=0;i<256;i++) {
			target[i] = inw(0x1F0);
        }
	}
}


void ata_write_sector(uint32_t lba, uint8_t* buffer) {

}

void ata_identify(uint16_t* buffer) {

}