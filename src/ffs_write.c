#include <assert.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"
#include "crc16.h"

static int
ffs_write_fill_crc16_overwrite(struct ffs_disk_block *disk_block,
                               uint8_t src_area_idx, uint32_t src_area_offset,
                               uint16_t left_copy_len, uint16_t right_copy_len,
                               const void *new_data, uint16_t new_data_len)
{
    uint16_t block_off;
    uint16_t crc16;
    int rc;

    block_off = 0;

    crc16 = ffs_crc_disk_block_hdr(disk_block);
    block_off += sizeof *disk_block;

    /* Copy data from the start of the old block, in case the new data starts
     * at a non-zero offset.
     */
    if (left_copy_len > 0) {
        rc = ffs_crc_flash(crc16, src_area_idx,
                                  src_area_offset + block_off,
                                  left_copy_len, &crc16);
        if (rc != 0) {
            return rc;
        }
        block_off += left_copy_len;
    }

    /* Write the new data into the data block.  This may extend the block's
     * length beyond its old value.
     */
    crc16 = crc16_ccitt(crc16, new_data, new_data_len);
    block_off += new_data_len;

    /* Copy data from the end of the old block, in case the new data doesn't
     * extend to the end of the block.
     */
    if (right_copy_len > 0) {
        rc = ffs_crc_flash(crc16, src_area_idx, src_area_offset + block_off,
                           right_copy_len, &crc16);
        if (rc != 0) {
            return rc;
        }
        block_off += right_copy_len;
    }

    assert(block_off == sizeof *disk_block + disk_block->fdb_data_len);

    disk_block->fdb_crc16 = crc16;

    return 0;
}

/**
 * Overwrites an existing data block.  The resulting block has the same ID as
 * the old one, but it supersedes it with a greater sequence number.
 *
 * @param entry                 The data block to overwrite.
 * @param left_copy_len         The number of bytes of existing data to retain
 *                                  before the new data begins.
 * @param new_data              The new data to write to the block.
 * @param new_data_len          The number of new bytes to write to the block.
 *                                  If this value plus left_copy_len is less
 *                                  than the existing block's data length,
 *                                  previous data at the end of the block is
 *                                  retained.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_write_over_block(struct ffs_hash_entry *entry, uint16_t left_copy_len,
                     const void *new_data, uint16_t new_data_len)
{
    struct ffs_disk_block disk_block;
    struct ffs_block block;
    uint32_t src_area_offset;
    uint32_t dst_area_offset;
    uint16_t right_copy_len;
    uint16_t block_off;
    uint8_t src_area_idx;
    uint8_t dst_area_idx;
    int rc;

    rc = ffs_block_from_hash_entry(&block, entry);
    if (rc != 0) {
        return rc;
    }

    assert(left_copy_len <= block.fb_data_len);

    /* Determine how much old data at the end of the block needs to be
     * retained.  If the new data doesn't extend to the end of the block, the
     * the rest of the block retains its old contents.
     */
    if (left_copy_len + new_data_len > block.fb_data_len) {
        right_copy_len = 0;
    } else {
        right_copy_len = block.fb_data_len - left_copy_len - new_data_len;
    }

    block.fb_seq++;
    block.fb_data_len = left_copy_len + new_data_len + right_copy_len;
    ffs_block_to_disk(&block, &disk_block);

    ffs_flash_loc_expand(entry->fhe_flash_loc,
                         &src_area_idx, &src_area_offset);

    rc = ffs_write_fill_crc16_overwrite(&disk_block,
                                        src_area_idx, src_area_offset,
                                        left_copy_len, right_copy_len,
                                        new_data, new_data_len);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_misc_reserve_space(sizeof disk_block + disk_block.fdb_data_len,
                                &dst_area_idx, &dst_area_offset);
    if (rc != 0) {
        return rc;
    }

    block_off = 0;

    /* Write the block header. */
    rc = ffs_flash_write(dst_area_idx, dst_area_offset + block_off,
                         &disk_block, sizeof disk_block);
    if (rc != 0) {
        return rc;
    }
    block_off += sizeof disk_block;

    /* Copy data from the start of the old block, in case the new data starts
     * at a non-zero offset.
     */
    if (left_copy_len > 0) {
        rc = ffs_flash_copy(src_area_idx, src_area_offset + block_off,
                            dst_area_idx, dst_area_offset + block_off,
                            left_copy_len);
        if (rc != 0) {
            return rc;
        }
        block_off += left_copy_len;
    }

    /* Write the new data into the data block.  This may extend the block's
     * length beyond its old value.
     */
    rc = ffs_flash_write(dst_area_idx, dst_area_offset + block_off,
                         new_data, new_data_len);
    if (rc != 0) {
        return rc;
    }
    block_off += new_data_len;

    /* Copy data from the end of the old block, in case the new data doesn't
     * extend to the end of the block.
     */
    if (right_copy_len > 0) {
        rc = ffs_flash_copy(src_area_idx, src_area_offset + block_off,
                            dst_area_idx, dst_area_offset + block_off,
                            right_copy_len);
        if (rc != 0) {
            return rc;
        }
        block_off += right_copy_len;
    }

    assert(block_off == sizeof disk_block + block.fb_data_len);

    entry->fhe_flash_loc = ffs_flash_loc(dst_area_idx, dst_area_offset);

#if FFS_DEBUG
    rc = ffs_crc_disk_block_validate(&disk_block, dst_area_idx,
                                     dst_area_offset);
    assert(rc == 0);
#endif

    return 0;
}

/**
 * Appends a new block to an inode block chain.
 *
 * @param inode_entry           The inode to append a block to.
 * @param data                  The contents of the new block.
 * @param len                   The number of bytes of data to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_write_append(struct ffs_cache_inode *cache_inode, const void *data,
                 uint16_t len)
{
    struct ffs_inode_entry *inode_entry;
    struct ffs_hash_entry *entry;
    struct ffs_disk_block disk_block;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    entry = ffs_block_entry_alloc();
    if (entry == NULL) {
        return FFS_ENOMEM;
    }

    inode_entry = cache_inode->fci_inode.fi_inode_entry;

    disk_block.fdb_magic = FFS_BLOCK_MAGIC;
    disk_block.fdb_id = ffs_hash_next_block_id++;
    disk_block.fdb_seq = 0;
    disk_block.fdb_inode_id = inode_entry->fie_hash_entry.fhe_id;
    if (inode_entry->fie_last_block_entry == NULL) {
        disk_block.fdb_prev_id = FFS_ID_NONE;
    } else {
        disk_block.fdb_prev_id = inode_entry->fie_last_block_entry->fhe_id;
    }
    disk_block.fdb_data_len = len;
    ffs_crc_disk_block_fill(&disk_block, data);

    rc = ffs_block_write_disk(&disk_block, data, &area_idx, &area_offset);
    if (rc != 0) {
        return rc;
    }

    entry->fhe_id = disk_block.fdb_id;
    entry->fhe_flash_loc = ffs_flash_loc(area_idx, area_offset);
    ffs_hash_insert(entry);

    inode_entry->fie_last_block_entry = entry;

    /* Update cached inode with the new file size. */
    cache_inode->fci_file_size += len;

    /* Add appended block to the cache. */
    ffs_cache_seek(cache_inode, cache_inode->fci_file_size - 1, NULL);

    return 0;
}

/**
 * Performs a single write operation.  The data written must be no greater
 * than the maximum block data length.  If old data gets overwritten, then
 * the existing data blocks are superseded as necessary.
 *
 * @param write_info            Describes the write operation being perfomred.
 * @param inode_entry           The file inode to write to.
 * @param data                  The new data to write.
 * @param data_len              The number of bytes of new data to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_write_chunk(struct ffs_inode_entry *inode_entry, uint32_t file_offset,
                const void *data, uint16_t data_len)
{
    struct ffs_cache_inode *cache_inode;
    struct ffs_cache_block *cache_block;
    uint32_t append_len;
    uint32_t data_offset;
    uint32_t block_end;
    uint32_t dst_off;
    uint16_t chunk_off;
    uint16_t chunk_sz;
    int rc;

    assert(data_len <= ffs_block_max_data_sz);

    rc = ffs_cache_inode_ensure(&cache_inode, inode_entry);
    if (rc != 0) {
        return rc;
    }

    /** Handle the simple append case first. */
    if (file_offset == cache_inode->fci_file_size) {
        rc = ffs_write_append(cache_inode, data, data_len);
        return rc;
    }

    /** This is not an append; i.e., old data is getting overwritten. */

    dst_off = file_offset + data_len;
    data_offset = data_len;
    cache_block = NULL;

    if (dst_off > cache_inode->fci_file_size) {
        append_len = dst_off - cache_inode->fci_file_size;
    } else {
        append_len = 0;
    }

    do {
        if (cache_block == NULL) {
            rc = ffs_cache_seek(cache_inode, dst_off - 1, &cache_block);
            if (rc != 0) {
                return rc;
            }
        }

        if (cache_block->fcb_file_offset < file_offset) {
            chunk_off = file_offset - cache_block->fcb_file_offset;
        } else {
            chunk_off = 0;
        }

        chunk_sz = cache_block->fcb_block.fb_data_len - chunk_off;
        block_end = cache_block->fcb_file_offset +
                    cache_block->fcb_block.fb_data_len;
        if (block_end != dst_off) {
            chunk_sz += (int)(dst_off - block_end);
        }

        data_offset = cache_block->fcb_file_offset + chunk_off - file_offset;
        rc = ffs_write_over_block(cache_block->fcb_block.fb_hash_entry,
                                  chunk_off, data + data_offset, chunk_sz);
        if (rc != 0) {
            return rc;
        }

        dst_off -= chunk_sz;
        cache_block = TAILQ_PREV(cache_block, ffs_cache_block_list, fcb_link);
    } while (data_offset > 0);

    cache_inode->fci_file_size += append_len;
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

    if (!(file->ff_access_flags & FFS_ACCESS_WRITE)) {
        return FFS_ERDONLY;
    }

    if (len == 0) {
        return 0;
    }

    /* The append flag forces all writes to the end of the file, regardless of
     * seek position.
     */
    if (file->ff_access_flags & FFS_ACCESS_APPEND) {
        rc = ffs_inode_data_len(file->ff_inode_entry, &file->ff_offset);
        if (rc != 0) {
            return rc;
        }
    }

    /* Write data as a sequence of blocks. */
    data_ptr = data;
    while (len > 0) {
        if (len > ffs_block_max_data_sz) {
            chunk_size = ffs_block_max_data_sz;
        } else {
            chunk_size = len;
        }

        rc = ffs_write_chunk(file->ff_inode_entry, file->ff_offset, data_ptr,
                           chunk_size);
        if (rc != 0) {
            return rc;
        }

        len -= chunk_size;
        data_ptr += chunk_size;
        file->ff_offset += chunk_size;
    }

    return 0;
}
