#include <stdint.h>

#ifndef ATA_H

#define ATA_H

#define DATA_REGISTER  0x1F0
#define ERROR_REGISTER 0x1F1
#define SECTOR_COUNT   0x1F2
#define LBA_LOW        0x1F3
#define LBA_MID        0x1F4
#define LBA_HIGH       0x1F5
#define DRIVE_HEAD     0x1F6
#define STATUS         0x1F7

#define STATUS_BSY     0x80
#define STATUS_DRQ     0x08
#define STATUS_ERR     0x01

int ata_init(void);
void ata_read_sector(uint32_t lba, uint8_t* buffer);
void ata_write_sector(uint32_t lba, uint8_t* buffer);
void ata_identify(uint16_t* buffer);

#endif