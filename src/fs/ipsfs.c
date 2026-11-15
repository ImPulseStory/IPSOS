#include "../include/ipsfs.h"

#include "../include/ata.h"

#include <stddef.h>

#include <stdint.h>

#define IPSFS_MAX_FILE_SIZE (BLOCK_SIZE * DIRECT_BLOCKS)

#define IPSFS_MAX_PATH 255

static ips_super_t g_super;

static void ips_memset(void *dst, uint8_t value, size_t count)

{

    uint8_t *p = (uint8_t *)dst;

    for (size_t i = 0; i < count; ++i)

        p[i] = value;

}

static void ips_memcpy(void *dst, const void *src, size_t count)

{

    uint8_t *d = (uint8_t *)dst;

    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0; i < count; ++i)

        d[i] = s[i];

}

static size_t ips_strlen(const char *s)

{

    size_t n = 0;

    if (!s)

        return 0;

    while (s[n] != '\0')

        ++n;

    return n;

}

static int ips_streq(const char *a, const char *b)

{

    size_t i = 0;

    if (!a || !b)

        return 0;

    while (a[i] && b[i] && a[i] == b[i])

        ++i;

    return a[i] == '\0' && b[i] == '\0';

}

static int ips_dirent_name_eq(const char stored[28], const char *name)

{

    size_t i = 0;

    if (!name)

        return 0;

    while (i < 28 && stored[i] && name[i] && stored[i] == name[i])

        ++i;

    return i < 28 && stored[i] == '\0' && name[i] == '\0';

}

static int ips_name_valid(const char *name)

{

    size_t len = ips_strlen(name);

    if (len == 0 || len >= sizeof(((ips_dirent_t *)0)->name))

        return 0;

    if (ips_streq(name, ".") || ips_streq(name, ".."))

        return 0;

    for (size_t i = 0; i < len; ++i) {

        if (name[i] == '/')

            return 0;

    }

    return 1;

}

static int ips_valid_inode_num(int inode_num)

{

    return inode_num > 0 && inode_num < (int)g_super.total_inodes &&

           inode_num < MAX_INODES;

}

static int ips_valid_data_block(uint32_t block)

{

    return block >= IPSFS_DATA_START && block < g_super.total_blocks &&

           block < (uint32_t)sizeof(g_super.block_bitmap) * 8u;

}

static int ips_bitmap_get(const uint8_t *bitmap, uint32_t bit)

{

    return (bitmap[bit / 8u] & (uint8_t)(1u << (bit % 8u))) != 0;

}

static void ips_bitmap_set(uint8_t *bitmap, uint32_t bit)

{

    bitmap[bit / 8u] |= (uint8_t)(1u << (bit % 8u));

}

static void ips_bitmap_clear(uint8_t *bitmap, uint32_t bit)

{

    bitmap[bit / 8u] &= (uint8_t)~(1u << (bit % 8u));

}

/* Raw block functions: buf MUST point to at least BLOCK_SIZE bytes. */

static void ips_read_block(uint32_t block, void *buf)

{

read_sector((unsigned int)(uintptr_t)buf, IPSFS_START_LBA + block, 1);

}

static void ips_write_block(uint32_t block, const void *buf)

{

ata_write_sector(IPSFS_START_LBA + block, (uint8_t *)buf, 1);

}

static void ips_load_super(void)
{
    uint8_t sectors[IPSFS_SUPER_BLOCKS * BLOCK_SIZE];
    for (uint32_t i = 0; i < IPSFS_SUPER_BLOCKS; ++i)
        ips_read_block(i, sectors + i * BLOCK_SIZE);
    ips_memcpy(&g_super, sectors, sizeof(g_super));
}

static void ips_save_super(void)
{
    uint8_t sectors[IPSFS_SUPER_BLOCKS * BLOCK_SIZE];
    ips_memset(sectors, 0, sizeof(sectors));
    ips_memcpy(sectors, &g_super, sizeof(g_super));
    for (uint32_t i = 0; i < IPSFS_SUPER_BLOCKS; ++i)
        ips_write_block(i, sectors + i * BLOCK_SIZE);
}

static void ips_read_inode(uint32_t num, ips_inode_t *out)

{

    uint8_t sector[BLOCK_SIZE];

    if (!out || num == 0 || num >= MAX_INODES)

        return;

ips_read_block(IPSFS_INODE_START + num, sector);

ips_memcpy(out, sector, sizeof(*out));

}

static void ips_write_inode(uint32_t num, const ips_inode_t *inode)

{

    uint8_t sector[BLOCK_SIZE];

    if (!inode || num == 0 || num >= MAX_INODES)

        return;

ips_memset(sector, 0, sizeof(sector));

ips_memcpy(sector, inode, sizeof(*inode));

ips_write_block(IPSFS_INODE_START + num, sector);

}

static int ips_alloc_inode(void)

{

    for (uint32_t i = 2; i < g_super.total_inodes && i < MAX_INODES; ++i) {

        if (!ips_bitmap_get(g_super.inode_bitmap, i)) {

ips_bitmap_set(g_super.inode_bitmap, i);

            return (int)i;

        }

    }

    return -1;

}

static int ips_alloc_block(void)

{

    for (uint32_t i = IPSFS_DATA_START; i < g_super.total_blocks; ++i) {

        if (!ips_bitmap_get(g_super.block_bitmap, i)) {

ips_bitmap_set(g_super.block_bitmap, i);

            return (int)i;

        }

    }

    return -1;

}

static void ips_free_block(uint32_t block)

{

    if (ips_valid_data_block(block))

ips_bitmap_clear(g_super.block_bitmap, block);

}

static void ips_free_inode(uint32_t inode_num)

{

    if (inode_num > 1 && inode_num < g_super.total_inodes && inode_num < MAX_INODES)

ips_bitmap_clear(g_super.inode_bitmap, inode_num);

}

static int ips_find_in_dir(uint32_t dir_inode, const char *name)

{

    ips_inode_t dir;

    uint8_t sector[BLOCK_SIZE];

    ips_dirent_t *entries = (ips_dirent_t *)sector;

    if (!ips_valid_inode_num((int)dir_inode) || !name)

        return -1;

ips_read_inode(dir_inode, &dir);

    if (dir.type != TYPE_DIR)

        return -1;

    for (int b = 0; b < DIRECT_BLOCKS; ++b) {

        if (dir.blocks[b] == 0)

            continue;

        if (!ips_valid_data_block(dir.blocks[b]))

            return -1;

ips_read_block(dir.blocks[b], sector);

        for (int i = 0; i < 16; ++i) {

            if (entries[i].inode == 0)

                continue;

            if (entries[i].inode >= g_super.total_inodes ||

                entries[i].inode >= MAX_INODES)

                continue;

            if (ips_dirent_name_eq(entries[i].name, name))

                return (int)entries[i].inode;

        }

    }

    return -1;

}

static int ips_add_to_dir(uint32_t dir_inode, const char *name, uint32_t target_inode)

{

    ips_inode_t dir;

    uint8_t sector[BLOCK_SIZE];

    ips_dirent_t *entries = (ips_dirent_t *)sector;

    if (!ips_valid_inode_num((int)dir_inode) ||

        !ips_valid_inode_num((int)target_inode) || !ips_name_valid(name))

        return -1;

ips_read_inode(dir_inode, &dir);

    if (dir.type != TYPE_DIR || ips_find_in_dir(dir_inode, name) != -1)

        return -1;

    for (int b = 0; b < DIRECT_BLOCKS; ++b) {

        if (dir.blocks[b] == 0) {

            int new_block = ips_alloc_block();

            if (new_block < 0)

                return -1;

            /* Persist allocation before exposing the block through the inode. */

ips_save_super();

ips_memset(sector, 0, sizeof(sector));

ips_write_block((uint32_t)new_block, sector);

            dir.blocks[b] = (uint32_t)new_block;

            dir.size += BLOCK_SIZE;

ips_write_inode(dir_inode, &dir);

        } else if (!ips_valid_data_block(dir.blocks[b])) {

            return -1;

        }

ips_read_block(dir.blocks[b], sector);

        for (int i = 0; i < 16; ++i) {

            if (entries[i].inode == 0) {

                entries[i].inode = target_inode;

ips_memset(entries[i].name, 0, sizeof(entries[i].name));

                size_t len = ips_strlen(name);

ips_memcpy(entries[i].name, name, len);

ips_write_block(dir.blocks[b], sector);

                return 0;

            }

        }

    }

    return -1;

}

static int ips_split_parent(const char *path, char *parent_path, char *name)

{

    size_t len;

    size_t slash = 0;

    size_t name_len;

    if (!path || path[0] != '/')

        return -1;

    len = ips_strlen(path);

    if (len < 2 || len > IPSFS_MAX_PATH)

        return -1;

    /* A trailing slash does not name a creatable/deletable entry. */

    if (path[len - 1] == '/')

        return -1;

    for (size_t i = 0; i < len; ++i) {

        if (path[i] == '/')

            slash = i;

    }

    name_len = len - slash - 1;

    if (name_len == 0 || name_len >= 28)

        return -1;

    for (size_t i = 0; i < slash; ++i)

        parent_path[i] = path[i];

    if (slash == 0) {

        parent_path[0] = '/';

        parent_path[1] = '\0';

    } else {

        parent_path[slash] = '\0';

    }

    for (size_t i = 0; i < name_len; ++i)

        name[i] = path[slash + 1 + i];

    name[name_len] = '\0';

    return ips_name_valid(name) ? 0 : -1;

}

int ips_resolve_path(const char *path)

{

    uint32_t current_inode = g_super.root_inode;

    size_t i = 0;

    if (!path || path[0] != '/' || g_super.root_inode == 0)

        return -1;

    while (path[i] == '/')

        ++i;

    if (path[i] == '\0')

        return (int)current_inode;

    while (path[i] != '\0') {

        char name[28];

        size_t name_len = 0;

        while (path[i] != '\0' && path[i] != '/') {

            if (name_len >= sizeof(name) - 1)

                return -1;

            name[name_len++] = path[i++];

        }

        name[name_len] = '\0';

        if (name_len == 0)

            return -1;

        int next = ips_find_in_dir(current_inode, name);

        if (next < 0)

            return -1;

        current_inode = (uint32_t)next;

        while (path[i] == '/')

            ++i;

    }

    return (int)current_inode;

}

static int ips_create_node(const char *path, uint8_t type)

{

    char parent_path[IPSFS_MAX_PATH + 1];

    char name[28];

    ips_inode_t parent;

    ips_inode_t node;

    int parent_num;

    int new_num;

    if (ips_split_parent(path, parent_path, name) != 0)

        return -1;

    if (ips_resolve_path(path) != -1)

        return -1;

    parent_num = ips_resolve_path(parent_path);

    if (parent_num < 0)

        return -1;

ips_read_inode((uint32_t)parent_num, &parent);

    if (parent.type != TYPE_DIR)

        return -1;

    new_num = ips_alloc_inode();

    if (new_num < 0)

        return -1;

ips_save_super();

ips_memset(&node, 0, sizeof(node));

    node.type = type;

ips_write_inode((uint32_t)new_num, &node);

    if (ips_add_to_dir((uint32_t)parent_num, name, (uint32_t)new_num) != 0) {

ips_free_inode((uint32_t)new_num);

ips_memset(&node, 0, sizeof(node));

ips_write_inode((uint32_t)new_num, &node);

ips_save_super();

        return -1;

    }

    return 0;

}

void ips_init(void)

{

ips_load_super();

    if (g_super.magic == IPSFS_MAGIC &&

        g_super.total_inodes > 1 && g_super.total_inodes <= MAX_INODES &&

        g_super.total_blocks > IPSFS_DATA_START &&

        g_super.total_blocks == IPSFS_TOTAL_BLOCKS &&

        g_super.root_inode == 1) {

        return;

    }

    /* New/invalid filesystem: initialize the IPSFS v2 layout at the configured disk offset. */

ips_memset(&g_super, 0, sizeof(g_super));

    g_super.magic = IPSFS_MAGIC;

    g_super.total_inodes = MAX_INODES;

    g_super.total_blocks = IPSFS_TOTAL_BLOCKS;

    g_super.root_inode = 1;

    /* inode 0 is the empty dirent sentinel; inode 1 is the root. */

ips_bitmap_set(g_super.inode_bitmap, 0);

ips_bitmap_set(g_super.inode_bitmap, 1);

    /* Reserve the superblock, inode area and the gap before data blocks. */

    for (uint32_t i = 0; i < IPSFS_DATA_START; ++i)

ips_bitmap_set(g_super.block_bitmap, i);

ips_save_super();

    ips_inode_t root;

ips_memset(&root, 0, sizeof(root));

    root.type = TYPE_DIR;

ips_write_inode(g_super.root_inode, &root);

}

int ips_mkdir(const char *path)

{

    return ips_create_node(path, TYPE_DIR);

}

int ips_create_file(const char *path)

{

    return ips_create_node(path, TYPE_FILE);

}

int ips_write_file(const char *path, const void *data, uint32_t size)

{

    int inode_num = ips_resolve_path(path);

    ips_inode_t inode;

    uint32_t new_blocks[DIRECT_BLOCKS];

    uint32_t old_blocks[DIRECT_BLOCKS];

    uint32_t needed;

    uint32_t allocated = 0;

    uint32_t bytes_written = 0;

    const uint8_t *bytes = (const uint8_t *)data;

    if (inode_num < 0 || size > IPSFS_MAX_FILE_SIZE || (size > 0 && !data))

        return -1;

ips_read_inode((uint32_t)inode_num, &inode);

    if (inode.type != TYPE_FILE)

        return -1;

    needed = (size + BLOCK_SIZE - 1u) / BLOCK_SIZE;

    for (uint32_t i = 0; i < DIRECT_BLOCKS; ++i) {

        old_blocks[i] = inode.blocks[i];

        new_blocks[i] = 0;

    }

    /* Reserve all replacement blocks before changing the inode. */

    for (uint32_t i = 0; i < needed; ++i) {

        int block = ips_alloc_block();

        if (block < 0) {

            for (uint32_t j = 0; j < allocated; ++j)

ips_free_block(new_blocks[j]);

ips_save_super();

            return -1;

        }

        new_blocks[allocated++] = (uint32_t)block;

    }

ips_save_super();

    for (uint32_t i = 0; i < needed; ++i) {

        uint8_t sector[BLOCK_SIZE];

        uint32_t chunk = size - bytes_written;

        if (chunk > BLOCK_SIZE)

            chunk = BLOCK_SIZE;

ips_memset(sector, 0, sizeof(sector));

ips_memcpy(sector, bytes + bytes_written, chunk);

ips_write_block(new_blocks[i], sector);

        bytes_written += chunk;

    }

    for (uint32_t i = 0; i < DIRECT_BLOCKS; ++i)

        inode.blocks[i] = new_blocks[i];

    inode.size = size;

ips_write_inode((uint32_t)inode_num, &inode);

    /* Old blocks are released only after the new inode is on disk. */

    for (uint32_t i = 0; i < DIRECT_BLOCKS; ++i) {

        if (old_blocks[i] != 0 && old_blocks[i] != new_blocks[i])

ips_free_block(old_blocks[i]);

    }

ips_save_super();

    return (int)bytes_written;

}

static int ips_directory_is_empty(const ips_inode_t *dir)

{

    uint8_t sector[BLOCK_SIZE];

    ips_dirent_t *entries = (ips_dirent_t *)sector;

    for (int b = 0; b < DIRECT_BLOCKS; ++b) {

        if (dir->blocks[b] == 0)

            continue;

        if (!ips_valid_data_block(dir->blocks[b]))

            return 0;

ips_read_block(dir->blocks[b], sector);

        for (int i = 0; i < 16; ++i) {

            if (entries[i].inode != 0)

                return 0;

        }

    }

    return 1;

}

int ips_delete(const char *path)

{

    char parent_path[IPSFS_MAX_PATH + 1];

    char name[28];

    int inode_num;

    int parent_num;

    ips_inode_t inode;

    ips_inode_t parent;

    uint8_t sector[BLOCK_SIZE];

    ips_dirent_t *entries = (ips_dirent_t *)sector;

    int found = 0;

    inode_num = ips_resolve_path(path);

    if (inode_num <= 1 || !ips_valid_inode_num(inode_num))

        return -1; /* Never delete root or inode 0. */

    if (ips_split_parent(path, parent_path, name) != 0)

        return -1;

    parent_num = ips_resolve_path(parent_path);

    if (parent_num < 0)

        return -1;

ips_read_inode((uint32_t)inode_num, &inode);

    if (inode.type != TYPE_FILE && inode.type != TYPE_DIR)

        return -1;

    if (inode.type == TYPE_DIR && !ips_directory_is_empty(&inode))

        return -1;

ips_read_inode((uint32_t)parent_num, &parent);

    if (parent.type != TYPE_DIR)

        return -1;

    /* Remove the directory reference before returning its inode/blocks. */

    for (int b = 0; b < DIRECT_BLOCKS && !found; ++b) {

        if (parent.blocks[b] == 0)

            continue;

        if (!ips_valid_data_block(parent.blocks[b]))

            return -1;

ips_read_block(parent.blocks[b], sector);

        for (int i = 0; i < 16; ++i) {

            if (entries[i].inode == (uint32_t)inode_num &&

ips_dirent_name_eq(entries[i].name, name)) {

                entries[i].inode = 0;

ips_memset(entries[i].name, 0, sizeof(entries[i].name));

ips_write_block(parent.blocks[b], sector);

                found = 1;

                break;

            }

        }

    }

    if (!found)

        return -1;

    for (int i = 0; i < DIRECT_BLOCKS; ++i) {

        if (inode.blocks[i] != 0)

ips_free_block(inode.blocks[i]);

    }

ips_free_inode((uint32_t)inode_num);

ips_memset(&inode, 0, sizeof(inode));

ips_write_inode((uint32_t)inode_num, &inode);

ips_save_super();

    return 0;

}

int ips_read_file(const char *path, void *buf, uint32_t max_size, uint32_t *out_size)

{

    int inode_num = ips_resolve_path(path);

    ips_inode_t inode;

    uint32_t to_read;

    uint32_t bytes_read = 0;

    uint8_t *out = (uint8_t *)buf;

    if (out_size)

        *out_size = 0;

    if (inode_num < 0 || (max_size > 0 && !buf))

        return -1;

ips_read_inode((uint32_t)inode_num, &inode);

    if (inode.type != TYPE_FILE || inode.size > IPSFS_MAX_FILE_SIZE)

        return -1;

    to_read = inode.size < max_size ? inode.size : max_size;

    for (uint32_t i = 0; bytes_read < to_read && i < DIRECT_BLOCKS; ++i) {

        uint8_t sector[BLOCK_SIZE];

        uint32_t chunk = to_read - bytes_read;

        if (chunk > BLOCK_SIZE)

            chunk = BLOCK_SIZE;

        if (!ips_valid_data_block(inode.blocks[i]))

            return -1;

ips_read_block(inode.blocks[i], sector);

ips_memcpy(out + bytes_read, sector, chunk);

        bytes_read += chunk;

    }

    if (bytes_read != to_read)

        return -1;

    if (out_size)

        *out_size = bytes_read;

    return 0;

}

int ips_list(const char *dir_path, char *out_buf, int max_len)

{

    int inode_num = ips_resolve_path(dir_path);

    ips_inode_t dir;

    uint8_t sector[BLOCK_SIZE];

    ips_dirent_t *entries = (ips_dirent_t *)sector;

    int buf_pos = 0;

    int count = 0;

    if (!out_buf || max_len <= 0 || inode_num < 0)

        return -1;

    out_buf[0] = '\0';

ips_read_inode((uint32_t)inode_num, &dir);

    if (dir.type != TYPE_DIR)

        return -2;

    for (int b = 0; b < DIRECT_BLOCKS; ++b) {

        if (dir.blocks[b] == 0)

            continue;

        if (!ips_valid_data_block(dir.blocks[b]))

            return -1;

ips_read_block(dir.blocks[b], sector);

        for (int i = 0; i < 16; ++i) {

            if (entries[i].inode == 0)

                continue;

            size_t name_len = 0;

            while (name_len < sizeof(entries[i].name) && entries[i].name[name_len])

                ++name_len;

            if (name_len == 0 || name_len == sizeof(entries[i].name))

                continue;

            if (buf_pos + (int)name_len + 2 > max_len)

                return -1;

ips_memcpy(out_buf + buf_pos, entries[i].name, name_len);

            buf_pos += (int)name_len;

            out_buf[buf_pos++] = '\n';

            ++count;

        }

    }

    out_buf[buf_pos] = '\0';

    return count;

}