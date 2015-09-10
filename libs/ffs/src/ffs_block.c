#include <stddef.h>
#include <assert.h>
#include <string.h>
#include "testutil/testutil.h"
#include "ffs/ffs.h"
#include "ffs_priv.h"
#include "crc16.h"

struct ffs_hash_entry *
ffs_block_entry_alloc(void)
{
    struct ffs_hash_entry *entry;

    entry = os_memblock_get(&ffs_block_entry_pool);
    if (entry != NULL) {
        memset(entry, 0, sizeof *entry);
    }

    return entry;
}

void
ffs_block_entry_free(struct ffs_hash_entry *entry)
{
    assert(ffs_hash_id_is_block(entry->fhe_id));
    os_memblock_put(&ffs_block_entry_pool, entry);
}

/**
 * Reads a data block header from flash.
 *
 * @param area_idx              The index of the area to read from.
 * @param area_offset           The offset within the area to read from.
 * @param out_disk_block        On success, the block header is writteh here.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_block_read_disk(uint8_t area_idx, uint32_t area_offset,
                    struct ffs_disk_block *out_disk_block)
{
    int rc;

    rc = ffs_flash_read(area_idx, area_offset, out_disk_block,
                        sizeof *out_disk_block);
    if (rc != 0) {
        return rc;
    }
    if (out_disk_block->fdb_magic != FFS_BLOCK_MAGIC) {
        return FFS_EUNEXP;
    }

    return 0;
}

/**
 * Writes the specified data block to a suitable location in flash.
 *
 * @param disk_block            Points to the disk block to write.
 * @param data                  The contents of the data block.
 * @param out_area_idx          On success, contains the index of the area
 *                                  written to.
 * @param out_area_offset       On success, contains the offset within the area
 *                                  written to.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_block_write_disk(const struct ffs_disk_block *disk_block,
                     const void *data,
                     uint8_t *out_area_idx, uint32_t *out_area_offset)
{
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    rc = ffs_misc_reserve_space(sizeof *disk_block + disk_block->fdb_data_len,
                                &area_idx, &area_offset);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_flash_write(area_idx, area_offset, disk_block,
                         sizeof *disk_block);
    if (rc != 0) {
        return rc;
    }

    if (disk_block->fdb_data_len > 0) {
        rc = ffs_flash_write(area_idx, area_offset + sizeof *disk_block,
                             data, disk_block->fdb_data_len);
        if (rc != 0) {
            return rc;
        }
    }

    *out_area_idx = area_idx;
    *out_area_offset = area_offset;

    ASSERT_IF_TEST(ffs_crc_disk_block_validate(disk_block, area_idx,
                                               area_offset) == 0);

    return 0;
}

static void
ffs_block_from_disk_no_ptrs(struct ffs_block *out_block,
                            const struct ffs_disk_block *disk_block)
{
    out_block->fb_seq = disk_block->fdb_seq;
    out_block->fb_inode_entry = NULL;
    out_block->fb_prev = NULL;
    out_block->fb_data_len = disk_block->fdb_data_len;
}

static int
ffs_block_from_disk(struct ffs_block *out_block,
                    const struct ffs_disk_block *disk_block,
                    uint8_t area_idx, uint32_t area_offset)
{
    ffs_block_from_disk_no_ptrs(out_block, disk_block);

    out_block->fb_inode_entry = ffs_hash_find_inode(disk_block->fdb_inode_id);
    if (out_block->fb_inode_entry == NULL) {
        return FFS_ECORRUPT;
    }

    if (disk_block->fdb_prev_id != FFS_ID_NONE) {
        out_block->fb_prev = ffs_hash_find_block(disk_block->fdb_prev_id);
        if (out_block->fb_prev == NULL) {
            return FFS_ECORRUPT;
        }
    }

    return 0;
}

/**
 * Constructs a disk-representation of the specified data block.
 *
 * @param block                 The source block to convert.
 * @param out_disk_block        The disk block to write to.
 */
void
ffs_block_to_disk(const struct ffs_block *block,
                  struct ffs_disk_block *out_disk_block)
{
    assert(block->fb_inode_entry != NULL);

    out_disk_block->fdb_magic = FFS_BLOCK_MAGIC;
    out_disk_block->fdb_id = block->fb_hash_entry->fhe_id;
    out_disk_block->fdb_seq = block->fb_seq;
    out_disk_block->fdb_inode_id =
        block->fb_inode_entry->fie_hash_entry.fhe_id;
    if (block->fb_prev == NULL) {
        out_disk_block->fdb_prev_id = FFS_ID_NONE;
    } else {
        out_disk_block->fdb_prev_id = block->fb_prev->fhe_id;
    }
    out_disk_block->fdb_data_len = block->fb_data_len;
}

/**
 * Deletes the specified block entry from the ffs RAM representation.
 *
 * @param block_entry           The block entry to delete.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_block_delete_from_ram(struct ffs_hash_entry *block_entry)
{
    struct ffs_block block;
    int rc;

    rc = ffs_block_from_hash_entry(&block, block_entry);
    if (rc != 0) {
        return rc;
    }

    assert(block.fb_inode_entry != NULL);
    if (block.fb_inode_entry->fie_last_block_entry == block_entry) {
        block.fb_inode_entry->fie_last_block_entry = block.fb_prev;
    }

    ffs_hash_remove(block_entry);
    ffs_block_entry_free(block_entry);

    return 0;
}

/**
 * Constructs a full data block representation from the specified minimal
 * block entry.  However, the resultant block's pointers are set to null,
 * rather than populated via hash table lookups.  This behavior is useful when
 * the RAM representation has not been fully constructed yet.
 *
 * @param out_block             On success, this gets populated with the data
 *                                  block information.
 * @param block_entry           The source block entry to convert.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_block_from_hash_entry_no_ptrs(struct ffs_block *out_block,
                                  struct ffs_hash_entry *block_entry)
{
    struct ffs_disk_block disk_block;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    assert(ffs_hash_id_is_block(block_entry->fhe_id));

    ffs_flash_loc_expand(block_entry->fhe_flash_loc, &area_idx, &area_offset);
    rc = ffs_block_read_disk(area_idx, area_offset, &disk_block);
    if (rc != 0) {
        return rc;
    }

    out_block->fb_hash_entry = block_entry;
    ffs_block_from_disk_no_ptrs(out_block, &disk_block);

    return 0;
}

/**
 * Constructs a full data block representation from the specified minimal block
 * entry.  The resultant block's pointers are populated via hash table lookups.
 *
 * @param out_block             On success, this gets populated with the data
 *                                  block information.
 * @param block_entry           The source block entry to convert.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_block_from_hash_entry(struct ffs_block *out_block,
                          struct ffs_hash_entry *block_entry)
{
    struct ffs_disk_block disk_block;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    assert(ffs_hash_id_is_block(block_entry->fhe_id));

    ffs_flash_loc_expand(block_entry->fhe_flash_loc, &area_idx, &area_offset);
    rc = ffs_block_read_disk(area_idx, area_offset, &disk_block);
    if (rc != 0) {
        return rc;
    }

    out_block->fb_hash_entry = block_entry;
    rc = ffs_block_from_disk(out_block, &disk_block, area_idx, area_offset);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
ffs_block_read_data(const struct ffs_block *block, uint16_t offset,
                    uint16_t length, void *dst)
{
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    ffs_flash_loc_expand(block->fb_hash_entry->fhe_flash_loc,
                         &area_idx, &area_offset);
    area_offset += sizeof (struct ffs_disk_block);
    area_offset += offset;

    rc = ffs_flash_read(area_idx, area_offset, dst, length);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
