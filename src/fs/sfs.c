#include "../include/sfs.h"
#include "../include/ata.h"
#include "../include/vga.h"
#include <stdint.h>


static int sfs_strcmp(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 1;
        a++; b++;
    }
    return (*a == *b) ? 0 : 1;
}

static void sfs_strcpy(char* dst, const char* src) {
    int i = 0;
    while (src[i] && i < SFS_NAME_LEN - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void sfs_load_super(sfs_super_t* out) {
    read_sector((unsigned int)out, SFS_SUPER_LBA, 1);
}

static void sfs_save_super(sfs_super_t* super) {
    ata_write_sector(SFS_SUPER_LBA, (uint8_t*)super, 1);
}

void sfs_init(void) {
    sfs_super_t super;
    sfs_load_super(&super);

    if (super.magic != SFS_MAGIC) {
        super.magic = SFS_MAGIC;
        super.version = SFS_VERSION;
        super.file_count = 0;
        super.next_free_lba = SFS_DATA_LBA;
        for (int i = 0; i < SFS_MAX_FILES; i++) {
            super.files[i].used = 0;
            super.files[i].name[0] = '\0';
            super.files[i].start_lba = 0;
            super.files[i].size = 0;
        }
        sfs_save_super(&super);
    }
}

int sfs_create(const char* name, uint8_t* data, uint32_t size) {
    sfs_super_t super;
    sfs_load_super(&super);

    for (int i = 0; i < SFS_MAX_FILES; i++) {
        if (super.files[i].used && sfs_strcmp(super.files[i].name, name) == 0) {
            super.files[i].used = 0;
            super.file_count--;
            break;
        }
    }
    int slot = -1;
    for (int i = 0; i < SFS_MAX_FILES; i++) {
        if (!super.files[i].used) { slot = i; break; }
    }
    if (slot == -1) return -1;

    uint32_t lba = super.next_free_lba;
    uint32_t sectors = (size + SFS_BLOCK - 1) / SFS_BLOCK;

    for (uint32_t i = 0; i < sectors; i++) {
        uint8_t tmp[512];
        for (int j = 0; j < 512; j++) tmp[j] = 0;

        uint32_t offset = i * SFS_BLOCK;
        uint32_t chunk = (size - offset < SFS_BLOCK) ? (size - offset) : SFS_BLOCK;
        for (uint32_t j = 0; j < chunk; j++) tmp[j] = data[offset + j];

        ata_write_sector(lba + i, tmp, 1);
    }

    sfs_strcpy(super.files[slot].name, name);
    super.files[slot].start_lba = lba;
    super.files[slot].size = size;
    super.files[slot].used = 1;

    super.next_free_lba = lba + sectors;
    super.file_count++;
    sfs_save_super(&super);

    return 0;
}

int sfs_read(const char* name, uint8_t* buf, uint32_t* size) {
    sfs_super_t super;
    sfs_load_super(&super);

    for (int i = 0; i < SFS_MAX_FILES; i++) {
        if (super.files[i].used && sfs_strcmp(super.files[i].name, name) == 0) {
            uint32_t sectors = (super.files[i].size + SFS_BLOCK - 1) / SFS_BLOCK;
            for (uint32_t s = 0; s < sectors; s++) {
                read_sector((unsigned int)buf + s * SFS_BLOCK, super.files[i].start_lba, 1);
            }
            *size = super.files[i].size;
            return 0;
        }
    }
    return -1;
}

void sfs_list(void) {
    sfs_super_t super;
    sfs_load_super(&super);

    if (super.file_count == 0) {
        print("(empty)\n");
        return;
    }

    for (int i = 0; i < SFS_MAX_FILES; i++) {
        if (super.files[i].used) {
            print(super.files[i].name);
            print("  ");
        }
    }
    print("\n");
}

int sfs_delete(const char* name) {
    sfs_super_t super;
    sfs_load_super(&super);

    for (int i = 0; i < SFS_MAX_FILES; i++) {
        if (super.files[i].used && sfs_strcmp(super.files[i].name, name) == 0) {
            super.files[i].used = 0;
            super.files[i].name[0] = '\0';
            super.file_count--;
            sfs_save_super(&super);
            return 0;
        }
    }
    return -1;
}

int sfs_create_text(const char* name, const char* text) {
    uint32_t size = 0;
    while (text[size] != '\0') size++;
    return sfs_create(name, (uint8_t*)text, size);
}