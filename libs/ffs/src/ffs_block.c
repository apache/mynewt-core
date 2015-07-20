#include <stddef.h>
#include <assert.h>
#include <string.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"

struct ffs_block *
ffs_block_alloc(void)
{
    struct ffs_block *block;

    block = os_memblock_get(&ffs_block_pool);
    if (block != NULL) {
        memset(block, 0, sizeof *block);
    }

    return block;
}

void
ffs_block_free(struct ffs_block *block)
{
    os_memblock_put(&ffs_block_pool, block);
}

int
ffs_block_read_disk(struct ffs_disk_block *out_disk_block, uint16_t sector_id,
                    uint32_t offset)
{
    int rc;

    rc = ffs_flash_read(sector_id, offset, out_disk_block,
                        sizeof *out_disk_block);
    if (rc != 0) {
        return rc;
    }
    if (out_disk_block->fdb_magic != FFS_BLOCK_MAGIC) {
        return FFS_EUNEXP;
    }

    return 0;
}

int
ffs_block_write_disk(uint16_t *out_sector_id, uint32_t *out_offset,
                     const struct ffs_disk_block *disk_block,
                     const void *data)
{
    uint32_t offset;
    uint16_t sector_id;
    int rc;

    rc = ffs_reserve_space(&sector_id, &offset,
                           sizeof *disk_block + disk_block->fdb_data_len);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_flash_write(sector_id, offset, disk_block, sizeof *disk_block);
    if (rc != 0) {
        return rc;
    }

    if (disk_block->fdb_data_len > 0) {
        rc = ffs_flash_write(sector_id, offset + sizeof *disk_block,
                             data, disk_block->fdb_data_len);
        if (rc != 0) {
            return rc;
        }
    }

    if (out_sector_id != NULL) {
        *out_sector_id = sector_id;
    }
    if (out_offset != NULL) {
        *out_offset = offset;
    }

    return 0;
}

void
ffs_block_from_disk(struct ffs_block *out_block,
                    const struct ffs_disk_block *disk_block,
                    uint16_t sector_id, uint32_t offset)
{
    out_block->fb_base.fb_type = FFS_OBJECT_TYPE_BLOCK;
    out_block->fb_base.fb_id = disk_block->fdb_id;
    out_block->fb_base.fb_seq = disk_block->fdb_seq;
    out_block->fb_base.fb_sector_id = sector_id;
    out_block->fb_base.fb_offset = offset;
    out_block->fb_rank = disk_block->fdb_rank;
    out_block->fb_data_len = disk_block->fdb_data_len;
}

void
ffs_block_delete_from_ram(struct ffs_block *block)
{
    ffs_hash_remove(&block->fb_base);

    if (block->fb_inode != NULL) {
        SLIST_REMOVE(&block->fb_inode->fi_block_list, block, ffs_block,
                     fb_next);
        block->fb_inode->fi_data_len -= block->fb_data_len;
    }

    ffs_block_free(block);
}

void
ffs_block_delete_list_from_ram(struct ffs_block *first, struct ffs_block *last)
{
    struct ffs_block *next;
    struct ffs_block *cur;

    cur = first;
    while (cur != NULL) {
        next = SLIST_NEXT(cur, fb_next);
        ffs_block_delete_from_ram(cur);
        if (cur == last) {
            break;
        }
        cur = next;
    }
}

int
ffs_block_delete_from_disk(const struct ffs_block *block)
{
    struct ffs_disk_block disk_block;
    int rc;

    memset(&disk_block, 0, sizeof disk_block);
    disk_block.fdb_magic = FFS_BLOCK_MAGIC;
    disk_block.fdb_id = block->fb_base.fb_id;
    disk_block.fdb_seq = block->fb_base.fb_seq + 1;
    disk_block.fdb_flags = FFS_BLOCK_F_DELETED;

    rc = ffs_block_write_disk(NULL, NULL, &disk_block, NULL);
    if (rc != 0) {
        return rc;
    }
}

void
ffs_block_delete_list_from_disk(const struct ffs_block *first,
                                const struct ffs_block *last)
{
    const struct ffs_block *next;
    const struct ffs_block *cur;

    cur = first;
    while (cur != NULL) {
        next = SLIST_NEXT(cur, fb_next);
        ffs_block_delete_from_disk(cur);
        if (cur == last) {
            break;
        }
        cur = next;
    }
}

