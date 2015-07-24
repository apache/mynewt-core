#include <assert.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

struct ffs_write_info {
    struct ffs_block *fwi_prev_block;
    struct ffs_block *fwi_start_block;
    struct ffs_block *fwi_end_block;
    uint32_t fwi_start_offset;
    uint32_t fwi_end_offset;
    uint32_t fwi_extra_length;
};

static int
ffs_write_calc_info(struct ffs_write_info *out_write_info,
                    const struct ffs_inode *inode,
                    uint32_t file_offset, uint32_t write_len)
{
    struct ffs_block *block;
    uint32_t block_offset;
    uint32_t chunk_len;
    int rc;

    rc = ffs_inode_seek(inode, file_offset,
                        &out_write_info->fwi_prev_block,
                        &out_write_info->fwi_start_block,
                        &out_write_info->fwi_start_offset);
    if (rc != 0) {
        return rc;
    }

    block_offset = out_write_info->fwi_start_offset;
    block = out_write_info->fwi_start_block;
    while (1) {
        if (block == NULL) {
            out_write_info->fwi_end_block = NULL;
            out_write_info->fwi_end_offset = 0;
            out_write_info->fwi_extra_length = write_len;
            return 0;
        }

        chunk_len = block->fb_data_len - block_offset;
        if (chunk_len > write_len) {
            out_write_info->fwi_end_block = block;
            out_write_info->fwi_end_offset = block_offset + write_len;
            out_write_info->fwi_extra_length = 0;
            return 0;
        }

        write_len -= chunk_len;
        block_offset = 0;
        block = SLIST_NEXT(block, fb_next);
    }
}

static uint32_t
ffs_write_disk_size(const struct ffs_write_info *write_info,
                    const struct ffs_disk_block *data_disk_block)
{
    const struct ffs_block *block;
    uint32_t size;

    /* New data disk block. */
    size = sizeof *data_disk_block + data_disk_block->fdb_data_len;

    /* Accumulate sizes of delete blocks. */
    if (write_info->fwi_start_block != NULL) {
        block = SLIST_NEXT(write_info->fwi_start_block, fb_next);
        while (1) {
            if (block == NULL) {
                break;
            }

            size += sizeof (struct ffs_disk_block);

            if (block == write_info->fwi_end_block) {
                break;
            }

            block = SLIST_NEXT(block, fb_next);
        }
    }

    return size;
}

static void
ffs_write_delete_block_list_from_ram(struct ffs_block *first,
                                     struct ffs_block *last)
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

static int
ffs_write_gen(const struct ffs_write_info *write_info, struct ffs_inode *inode,
              const void *data, int data_len)
{
    struct ffs_disk_block disk_block;
    struct ffs_block *block;
    struct ffs_block *next;
    uint32_t expected_cur;
    uint32_t disk_size;
    uint32_t chunk_len;
    uint32_t offset;
    uint32_t cur;
    uint16_t sector_id;
    int rc;

    disk_block.fdb_data_len = data_len;
    if (write_info->fwi_start_block != NULL) {
        disk_block.fdb_data_len += write_info->fwi_start_offset;
        disk_block.fdb_id = write_info->fwi_start_block->fb_base.fb_id;
        disk_block.fdb_seq = write_info->fwi_start_block->fb_base.fb_seq + 1;
        disk_block.fdb_rank = write_info->fwi_start_block->fb_rank;
    } else {
        disk_block.fdb_id = ffs_next_id++;
        disk_block.fdb_seq = 0;
        if (write_info->fwi_prev_block != NULL) {
            disk_block.fdb_rank = write_info->fwi_prev_block->fb_rank + 1;
        } else {
            disk_block.fdb_rank = 0;
        }
    }

    if (write_info->fwi_end_block != NULL) {
        disk_block.fdb_data_len += write_info->fwi_end_block->fb_data_len -
                                   write_info->fwi_end_offset;
    }

    disk_block.fdb_magic = FFS_BLOCK_MAGIC;
    disk_block.fdb_inode_id = inode->fi_base.fb_id;
    disk_block.reserved16 = 0;
    disk_block.fdb_flags = 0;
    disk_block.fdb_ecc = 0;

    disk_size = ffs_write_disk_size(write_info, &disk_block);
    rc = ffs_misc_reserve_space(&sector_id, &offset, disk_size);
    if (rc != 0) {
        return rc;
    }
    expected_cur = ffs_sectors[sector_id].fs_cur + disk_size;

    rc = ffs_flash_write(sector_id, offset, &disk_block, sizeof disk_block);
    if (rc != 0) {
        return rc;
    }

    cur = offset + sizeof disk_block;

    if (write_info->fwi_start_block != NULL) {
        chunk_len = write_info->fwi_start_offset;
        rc = ffs_flash_copy(
            write_info->fwi_start_block->fb_base.fb_sector_id,
            write_info->fwi_start_block->fb_base.fb_offset + sizeof disk_block,
            sector_id, cur, chunk_len);
        if (rc != 0) {
            return rc;
        }
        cur += chunk_len;
    }

    rc = ffs_flash_write(sector_id, cur, data, data_len);
    if (rc != 0) {
        return rc;
    }

    cur += data_len;

    if (write_info->fwi_end_block != NULL) {
        chunk_len = write_info->fwi_end_block->fb_data_len -
                    write_info->fwi_end_offset;
        rc = ffs_flash_copy(
            write_info->fwi_end_block->fb_base.fb_sector_id,
            write_info->fwi_end_block->fb_base.fb_offset +
                sizeof disk_block + write_info->fwi_end_offset,
            sector_id, cur, chunk_len);
        if (rc != 0) {
            return rc;
        }
        cur += chunk_len;
    }

    assert(cur - offset == sizeof disk_block + disk_block.fdb_data_len);

    if (write_info->fwi_start_block != NULL) {
        next = SLIST_NEXT(write_info->fwi_start_block, fb_next);
        if (next != NULL) {
            ffs_block_delete_list_from_disk(next,
                                            write_info->fwi_end_block);
        }
        ffs_write_delete_block_list_from_ram(write_info->fwi_start_block,
                                             write_info->fwi_end_block);
    }

    block = ffs_block_alloc();
    if (block == NULL) {
        return FFS_ENOMEM;
    }

    ffs_block_from_disk(block, &disk_block, sector_id, offset);
    block->fb_inode = inode;
    ffs_hash_insert(&block->fb_base);
    ffs_inode_insert_block(inode, block);

    assert(ffs_sectors[sector_id].fs_cur == expected_cur);

    return 0;
}

static int
ffs_write_chunk(struct ffs_file *file, const void *data, int len)
{
    struct ffs_write_info write_info;
    uint32_t file_offset;
    int rc;

    assert(len <= FFS_BLOCK_MAX_DATA_SZ);

    if (!(file->ff_access_flags & FFS_ACCESS_WRITE)) {
        return FFS_ERDONLY;
    }

    /* The append flag forces all writes to the end of the file, regardless of
     * seek position.
     */
    if (file->ff_access_flags & FFS_ACCESS_APPEND) {
        file_offset = file->ff_inode->fi_data_len;
    } else {
        file_offset = file->ff_offset;
    }

    rc = ffs_write_calc_info(&write_info, file->ff_inode, file_offset, len);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_write_gen(&write_info, file->ff_inode, data, len);
    if (rc != 0) {
        return rc;
    }

    /* A write always results in a seek position one past the end of the
     * write.
     */
    file->ff_offset = file_offset + len;

    return 0;
}

int
ffs_write_to_file(struct ffs_file *file, const void *data, int len)
{
    const uint8_t *data_ptr;
    uint16_t chunk_size;
    int rc;

    /* Write data as a sequence of blocks. */
    data_ptr = data;
    while (len > 0) {
        if (len > FFS_BLOCK_MAX_DATA_SZ) {
            chunk_size = FFS_BLOCK_MAX_DATA_SZ;
        } else {
            chunk_size = len;
        }

        rc = ffs_write_chunk(file, data_ptr, chunk_size);
        if (rc != 0) {
            return rc;
        }

        len -= chunk_size;
        data_ptr += chunk_size;
    }

    return 0;
}

