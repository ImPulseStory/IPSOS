#ifndef IPSFS_H
#define IPSFS_H

#include <stdint.h>

#define IPSFS_START_LBA          2048u
#define IPSFS_TOTAL_BLOCKS       18432u

#define IPSFS_MAGIC              0x49505346u
#define MAX_INODES               128u
#define BLOCK_SIZE               512u
#define DIRECT_BLOCKS            126u

#define IPSFS_SUPER_BLOCKS       6u
#define IPSFS_INODE_START        IPSFS_SUPER_BLOCKS
#define IPSFS_DATA_START         (IPSFS_INODE_START + MAX_INODES)
#define IPSFS_BLOCK_BITMAP_BYTES ((IPSFS_TOTAL_BLOCKS + 7u) / 8u)

#define TYPE_FREE                0u
#define TYPE_FILE                1u
#define TYPE_DIR                 2u

typedef struct {
    uint8_t type;
    uint32_t size;
    uint32_t blocks[DIRECT_BLOCKS];
} ips_inode_t;

typedef struct {
    uint32_t inode;
    char name[28];
} ips_dirent_t;

/* Stored in six sectors. Block numbers are relative to IPSFS_START_LBA. */
typedef struct {
    uint32_t magic;
    uint32_t total_inodes;
    uint32_t total_blocks;
    uint32_t root_inode;
    uint8_t inode_bitmap[16];
    uint8_t block_bitmap[IPSFS_BLOCK_BITMAP_BYTES];
} ips_super_t;

void ips_init(void);
int ips_resolve_path(const char *path);
int ips_mkdir(const char *path);
int ips_create_file(const char *path);
int ips_write_file(const char *path, const void *data, uint32_t size);
int ips_read_file(const char *path, void *buf, uint32_t max_size, uint32_t *out_size);
int ips_delete(const char *path);
int ips_list(const char *dir_path, char *out_buf, int max_len);

#endif
