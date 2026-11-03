#ifndef SFS_H
#define SFS_H

#include <stdint.h>

#define SFS_MAGIC     0x534653  // 'SFS'
#define SFS_VERSION   1
#define SFS_BLOCK     512
#define SFS_MAX_FILES 8
#define SFS_NAME_LEN  16

#define SFS_SUPER_LBA 200
#define SFS_DATA_LBA  201

typedef struct {
    char     name[SFS_NAME_LEN];
    uint32_t start_lba;
    uint32_t size;
    uint8_t  used;
} __attribute__((packed)) sfs_file_t;

typedef struct {
    uint32_t   magic;
    uint32_t   version;
    uint32_t   file_count;
    uint32_t   next_free_lba;
    sfs_file_t files[SFS_MAX_FILES];
    uint8_t    padding[512 - 16 - (SFS_MAX_FILES * (SFS_NAME_LEN + 4 + 4 + 1))];
} __attribute__((packed)) sfs_super_t;

void sfs_init(void);
int  sfs_create(const char* name, uint8_t* data, uint32_t size);
int  sfs_read(const char* name, uint8_t* buf, uint32_t* size);
void sfs_list(void);
int  sfs_delete(const char* name);

#endif