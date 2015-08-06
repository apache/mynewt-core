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

    block->fb_object.fo_type = FFS_OBJECT_TYPE_BLOCK;

    return block;
}

void
ffs_block_free(struct ffs_block *block)
{
    os_memblock_put(&ffs_block_pool, block);
}

uint32_t
ffs_block_disk_size(const struct ffs_block *block)
{
    return sizeof (struct ffs_disk_block) + block->fb_data_len;
}

int
ffs_block_read_disk(struct ffs_disk_block *out_disk_block, uint8_t area_idx,
                    uint32_t offset)
{
    int rc;

    rc = ffs_flash_read(area_idx, offset, out_disk_block,
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
ffs_block_write_disk(uint8_t *out_area_idx, uint32_t *out_offset,
                     const struct ffs_disk_block *disk_block,
                     const void *data)
{
    uint32_t offset;
    uint8_t area_idx;
    int rc;

    rc = ffs_misc_reserve_space(&area_idx, &offset,
                                sizeof *disk_block + disk_block->fdb_data_len);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_flash_write(area_idx, offset, disk_block, sizeof *disk_block);
    if (rc != 0) {
        return rc;
    }

    if (disk_block->fdb_data_len > 0) {
        rc = ffs_flash_write(area_idx, offset + sizeof *disk_block,
                             data, disk_block->fdb_data_len);
        if (rc != 0) {
            return rc;
        }
    }

    if (out_area_idx != NULL) {
        *out_area_idx = area_idx;
    }
    if (out_offset != NULL) {
        *out_offset = offset;
    }

    return 0;
}

void
ffs_block_from_disk(struct ffs_block *out_block,
                    const struct ffs_disk_block *disk_block,
                    uint8_t area_idx, uint32_t offset)
{
    out_block->fb_object.fo_type = FFS_OBJECT_TYPE_BLOCK;
    out_block->fb_object.fo_id = disk_block->fdb_id;
    out_block->fb_object.fo_seq = disk_block->fdb_seq;
    out_block->fb_object.fo_area_idx = area_idx;
    out_block->fb_object.fo_area_offset = offset;
    out_block->fb_rank = disk_block->fdb_rank;
    out_block->fb_data_len = disk_block->fdb_data_len;
}

void
ffs_block_delete_from_ram(struct ffs_block *block)
{
    ffs_hash_remove(&block->fb_object);

    if (block->fb_inode != NULL) {
        SLIST_REMOVE(&block->fb_inode->fi_block_list, block, ffs_block,
                     fb_next);
    }

    ffs_block_free(block);
}


int
ffs_block_delete_from_disk(const struct ffs_block *block)
{
    struct ffs_disk_block disk_block;
    uint8_t area_idx;
    int rc;

    memset(&disk_block, 0, sizeof disk_block);
    disk_block.fdb_magic = FFS_BLOCK_MAGIC;
    disk_block.fdb_id = block->fb_object.fo_id;
    disk_block.fdb_seq = block->fb_object.fo_seq + 1;
    disk_block.fdb_flags = FFS_BLOCK_F_DELETED;

    rc = ffs_block_write_disk(&area_idx, NULL, &disk_block, NULL);
    if (rc != 0) {
        return rc;
    }

    return 0;
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

