#include <assert.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

/**
 * Structure describing an individual write operation.  Indicates which blocks
 * get overwritten and at what offsets.
 */
struct ffs_write_info {
    /* The data block immediately before the one being written. */
    struct ffs_block *fwi_prev_block;

    /* The first data block being overwritten; null if no overwrite. */
    struct ffs_block *fwi_start_block;

    /* The last data block being overwritten (incl.); null if no overwrite. */
    struct ffs_block *fwi_end_block;

    /* The offset within the first overwritten block where the new write
     * begins; undefined if no overwrite.
     */
    uint32_t fwi_start_offset;

    /* The offset within the last overwritten block where the new write ends;
     * undefined if no overwrite.
     */
    uint32_t fwi_end_offset;

    /* The amount of data being appended to the file.  This is equal to the
     * total length of the write minus all overwritten bytes.
     */
    uint32_t fwi_extra_length;
};

/**
 * Calculates a write info struct corresponding to the specified write
 * operation.
 *
 * @param out_write_info        On success, this gets populated.
 * @param inode                 The inode corresponding to the file being
 *                                  written to.
 * @param file_offset           The file offset of the start of the write.
 * @param write_len             The number of bytes to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_write_calc_info(struct ffs_write_info *out_write_info,
                    const struct ffs_inode *inode,
                    uint32_t file_offset, uint32_t write_len)
{
    struct ffs_block *block;
    uint32_t block_offset;
    uint32_t chunk_len;
    int rc;

    /* Find the first block being overwritten along with its predecessor. */
    rc = ffs_inode_seek(inode, file_offset,
                        &out_write_info->fwi_prev_block,
                        &out_write_info->fwi_start_block,
                        &out_write_info->fwi_start_offset);
    if (rc != 0) {
        return rc;
    }

    /* Find the last block being overwritten; iterate through blocks in the
     * file until either:
     *     o we have reached the end of the file (i.e., no overwrite or partial
     *       overwrite).
     *     o we have advanced the full 'write_len' bytes (i.e., the new write
     *       only overwrites existing data); or
     */
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

/**
 * Calculates the amount of disk space required for the specified write
 * operation.
 *
 * @param write_info            The write operation to measure.
 * @param data_disk_block       The disk block header for the write.
 *
 * @return                      The amount of required disk space.
 */
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

/**
 * Deletes from ram the specified list of data blocks, inclusive.
 */
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

/**
 * Performs a single write operation, overwriting existing data blocks as
 * necessary.  A write operation can specify no more than a single block's
 * worth of data.  That is, the length of data must not exceed the maximum
 * block size.
 *
 * @param write_info            Describes the write operation to perform;
 *                                  calculated via a call to
 *                                  ffs_write_calc_info.
 * @param inode                 The inode corresponding to the file being
 *                                  written to.
 * @param data                  The raw data to write to the file.
 * @param data_len              The number of bytes to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_write_gen(const struct ffs_write_info *write_info, struct ffs_inode *inode,
              const void *data, int data_len)
{
    struct ffs_disk_block disk_block;
    struct ffs_block *block;
    struct ffs_block *next;
    uint32_t disk_size;
    uint32_t chunk_len;
    uint32_t offset;
    uint32_t cur;
    uint8_t area_idx;
    int rc;

    assert(data_len <= ffs_block_max_data_sz);

    /* Build the disk block header corresponding to the write. */
    disk_block.fdb_data_len = data_len;
    if (write_info->fwi_start_block != NULL) {
        /* One or more blocks are being overwritten.  Supersede the first
         * overwritten block with the new one.
         */
        disk_block.fdb_id = write_info->fwi_start_block->fb_object.fo_id;
        disk_block.fdb_seq = write_info->fwi_start_block->fb_object.fo_seq + 1;
        disk_block.fdb_rank = write_info->fwi_start_block->fb_rank;

        /* Add the non-overwritten part of the block to the total length. */
        disk_block.fdb_data_len += write_info->fwi_start_offset;
    } else {
        /* No overwrite; allocate a new object ID. */
        disk_block.fdb_id = ffs_next_id++;
        disk_block.fdb_seq = 0;
        if (write_info->fwi_prev_block != NULL) {
            disk_block.fdb_rank = write_info->fwi_prev_block->fb_rank + 1;
        } else {
            disk_block.fdb_rank = 0;
        }
    }

    if (write_info->fwi_end_block != NULL) {
        /* This is a complete overwrite.  Add the leftovers at the end to the
         * total length.
         */
        disk_block.fdb_data_len += write_info->fwi_end_block->fb_data_len -
                                   write_info->fwi_end_offset;
    }
    disk_block.fdb_magic = FFS_BLOCK_MAGIC;
    disk_block.fdb_inode_id = inode->fi_object.fo_id;
    disk_block.fdb_flags = 0;

    /* Write the new data block to disk. */
    disk_size = ffs_write_disk_size(write_info, &disk_block);
    rc = ffs_misc_reserve_space(&area_idx, &offset, disk_size);
    if (rc != 0) {
        return rc;
    }
    rc = ffs_flash_write(area_idx, offset, &disk_block, sizeof disk_block);
    if (rc != 0) {
        return rc;
    }

    cur = offset + sizeof disk_block;

    if (write_info->fwi_start_block != NULL) {
        chunk_len = write_info->fwi_start_offset;
        rc = ffs_flash_copy(
            write_info->fwi_start_block->fb_object.fo_area_idx,
            write_info->fwi_start_block->fb_object.fo_area_offset +
                sizeof disk_block,
            area_idx, cur, chunk_len);
        if (rc != 0) {
            return rc;
        }
        cur += chunk_len;
    }

    rc = ffs_flash_write(area_idx, cur, data, data_len);
    if (rc != 0) {
        return rc;
    }

    cur += data_len;

    if (write_info->fwi_end_block != NULL) {
        chunk_len = write_info->fwi_end_block->fb_data_len -
                    write_info->fwi_end_offset;
        rc = ffs_flash_copy(
            write_info->fwi_end_block->fb_object.fo_area_idx,
            write_info->fwi_end_block->fb_object.fo_area_offset +
                sizeof disk_block + write_info->fwi_end_offset,
            area_idx, cur, chunk_len);
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

    ffs_block_from_disk(block, &disk_block, area_idx, offset);
    block->fb_inode = inode;
    ffs_hash_insert(&block->fb_object);
    ffs_inode_insert_block(inode, block);

    return 0;
}

/**
 * Writes a size-constrained chunk of contiguous data to a file.  The chunk
 * must not be larger than the maximum block data size.
 *
 * @param file                  The file to write to.
 * @param data                  The data to write.
 * @param len                   The length of data to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_write_chunk(struct ffs_file *file, const void *data, int len)
{
    struct ffs_write_info write_info;
    uint32_t file_offset;
    int rc;

    assert(len <= ffs_block_max_data_sz);

    if (!(file->ff_access_flags & FFS_ACCESS_WRITE)) {
        return FFS_ERDONLY;
    }

    /* The append flag forces all writes to the end of the file, regardless of
     * seek position.
     */
    if (file->ff_access_flags & FFS_ACCESS_APPEND) {
        file_offset = ffs_inode_calc_data_length(file->ff_inode);
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

/**
 * Writes a chunk of contiguous data to a file.
 *
 * @param file                  The file to write to.
 * @param data                  The data to write.
 * @param len                   The length of data to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_write_to_file(struct ffs_file *file, const void *data, int len)
{
    const uint8_t *data_ptr;
    uint16_t chunk_size;
    int rc;

    /* Write data as a sequence of blocks. */
    data_ptr = data;
    while (len > 0) {
        if (len > ffs_block_max_data_sz) {
            chunk_size = ffs_block_max_data_sz;
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

