/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include "testutil/testutil.h"
#include "nffs/nffs.h"
#include "nffs_priv.h"
#include "crc16.h"

struct nffs_hash_entry *
nffs_block_entry_alloc(void)
{
    struct nffs_hash_entry *entry;

    entry = os_memblock_get(&nffs_block_entry_pool);
    if (entry != NULL) {
        memset(entry, 0, sizeof *entry);
    }

    return entry;
}

void
nffs_block_entry_free(struct nffs_hash_entry *entry)
{
    assert(nffs_hash_id_is_block(entry->nhe_id));
    os_memblock_put(&nffs_block_entry_pool, entry);
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
nffs_block_read_disk(uint8_t area_idx, uint32_t area_offset,
                     struct nffs_disk_block *out_disk_block)
{
    int rc;

    rc = nffs_flash_read(area_idx, area_offset, out_disk_block,
                        sizeof *out_disk_block);
    if (rc != 0) {
        return rc;
    }
    if (out_disk_block->ndb_magic != NFFS_BLOCK_MAGIC) {
        return NFFS_EUNEXP;
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
nffs_block_write_disk(const struct nffs_disk_block *disk_block,
                      const void *data,
                      uint8_t *out_area_idx, uint32_t *out_area_offset)
{
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    rc = nffs_misc_reserve_space(sizeof *disk_block + disk_block->ndb_data_len,
                                &area_idx, &area_offset);
    if (rc != 0) {
        return rc;
    }

    rc = nffs_flash_write(area_idx, area_offset, disk_block,
                         sizeof *disk_block);
    if (rc != 0) {
        return rc;
    }

    if (disk_block->ndb_data_len > 0) {
        rc = nffs_flash_write(area_idx, area_offset + sizeof *disk_block,
                             data, disk_block->ndb_data_len);
        if (rc != 0) {
            return rc;
        }
    }

    *out_area_idx = area_idx;
    *out_area_offset = area_offset;

    ASSERT_IF_TEST(nffs_crc_disk_block_validate(disk_block, area_idx,
                                               area_offset) == 0);

    return 0;
}

static void
nffs_block_from_disk_no_ptrs(struct nffs_block *out_block,
                             const struct nffs_disk_block *disk_block)
{
    out_block->nb_seq = disk_block->ndb_seq;
    out_block->nb_inode_entry = NULL;
    out_block->nb_prev = NULL;
    out_block->nb_data_len = disk_block->ndb_data_len;
}

static int
nffs_block_from_disk(struct nffs_block *out_block,
                     const struct nffs_disk_block *disk_block,
                     uint8_t area_idx, uint32_t area_offset)
{
    nffs_block_from_disk_no_ptrs(out_block, disk_block);

    out_block->nb_inode_entry = nffs_hash_find_inode(disk_block->ndb_inode_id);
    if (out_block->nb_inode_entry == NULL) {
        return NFFS_ECORRUPT;
    }

    if (disk_block->ndb_prev_id != NFFS_ID_NONE) {
        out_block->nb_prev = nffs_hash_find_block(disk_block->ndb_prev_id);
        if (out_block->nb_prev == NULL) {
            return NFFS_ECORRUPT;
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
nffs_block_to_disk(const struct nffs_block *block,
                   struct nffs_disk_block *out_disk_block)
{
    assert(block->nb_inode_entry != NULL);

    out_disk_block->ndb_magic = NFFS_BLOCK_MAGIC;
    out_disk_block->ndb_id = block->nb_hash_entry->nhe_id;
    out_disk_block->ndb_seq = block->nb_seq;
    out_disk_block->ndb_inode_id =
        block->nb_inode_entry->nie_hash_entry.nhe_id;
    if (block->nb_prev == NULL) {
        out_disk_block->ndb_prev_id = NFFS_ID_NONE;
    } else {
        out_disk_block->ndb_prev_id = block->nb_prev->nhe_id;
    }
    out_disk_block->ndb_data_len = block->nb_data_len;
}

/**
 * Deletes the specified block entry from the nffs RAM representation.
 *
 * @param block_entry           The block entry to delete.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
nffs_block_delete_from_ram(struct nffs_hash_entry *block_entry)
{
    struct nffs_block block;
    int rc;

    rc = nffs_block_from_hash_entry(&block, block_entry);
    if (rc != 0) {
        return rc;
    }

    assert(block.nb_inode_entry != NULL);
    if (block.nb_inode_entry->nie_last_block_entry == block_entry) {
        block.nb_inode_entry->nie_last_block_entry = block.nb_prev;
    }

    nffs_hash_remove(block_entry);
    nffs_block_entry_free(block_entry);

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
nffs_block_from_hash_entry_no_ptrs(struct nffs_block *out_block,
                                   struct nffs_hash_entry *block_entry)
{
    struct nffs_disk_block disk_block;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    assert(nffs_hash_id_is_block(block_entry->nhe_id));

    nffs_flash_loc_expand(block_entry->nhe_flash_loc, &area_idx, &area_offset);
    rc = nffs_block_read_disk(area_idx, area_offset, &disk_block);
    if (rc != 0) {
        return rc;
    }

    out_block->nb_hash_entry = block_entry;
    nffs_block_from_disk_no_ptrs(out_block, &disk_block);

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
nffs_block_from_hash_entry(struct nffs_block *out_block,
                           struct nffs_hash_entry *block_entry)
{
    struct nffs_disk_block disk_block;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    assert(nffs_hash_id_is_block(block_entry->nhe_id));

    nffs_flash_loc_expand(block_entry->nhe_flash_loc, &area_idx, &area_offset);
    rc = nffs_block_read_disk(area_idx, area_offset, &disk_block);
    if (rc != 0) {
        return rc;
    }

    out_block->nb_hash_entry = block_entry;
    rc = nffs_block_from_disk(out_block, &disk_block, area_idx, area_offset);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
nffs_block_read_data(const struct nffs_block *block, uint16_t offset,
                     uint16_t length, void *dst)
{
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    nffs_flash_loc_expand(block->nb_hash_entry->nhe_flash_loc,
                         &area_idx, &area_offset);
    area_offset += sizeof (struct nffs_disk_block);
    area_offset += offset;

    rc = nffs_flash_read(area_idx, area_offset, dst, length);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
