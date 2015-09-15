/**
 * Copyright (c) 2015 Stack Inc.
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

#include <assert.h>
#include <string.h>
#include "os/os_malloc.h"
#include "testutil/testutil.h"
#include "ffs_priv.h"
#include "ffs/ffs.h"

static int
ffs_gc_copy_object(struct ffs_hash_entry *entry, uint16_t object_size,
                   uint8_t to_area_idx)
{
    uint32_t from_area_offset;
    uint32_t to_area_offset;
    uint8_t from_area_idx;
    int rc;

    ffs_flash_loc_expand(entry->fhe_flash_loc,
                         &from_area_idx, &from_area_offset);
    to_area_offset = ffs_areas[to_area_idx].fa_cur;

    rc = ffs_flash_copy(from_area_idx, from_area_offset, to_area_idx,
                        to_area_offset, object_size);
    if (rc != 0) {
        return rc;
    }

    entry->fhe_flash_loc = ffs_flash_loc(to_area_idx, to_area_offset);

    return 0;
}

static int
ffs_gc_copy_inode(struct ffs_inode_entry *inode_entry, uint8_t to_area_idx)
{
    struct ffs_inode inode;
    uint16_t copy_len;
    int rc;

    rc = ffs_inode_from_entry(&inode, inode_entry);
    if (rc != 0) {
        return rc;
    }
    copy_len = sizeof (struct ffs_disk_inode) + inode.fi_filename_len;

    rc = ffs_gc_copy_object(&inode_entry->fie_hash_entry, copy_len,
                            to_area_idx);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Selects the most appropriate area for garbage collection.
 *
 * @return                  The ID of the area to garbage collect.
 */
static uint16_t
ffs_gc_select_area(void)
{
    const struct ffs_area *area;
    uint8_t best_area_idx;
    int8_t diff;
    int i;

    best_area_idx = 0;
    for (i = 1; i < ffs_num_areas; i++) {
        if (i == ffs_scratch_area_idx) {
            continue;
        }

        area = ffs_areas + i;
        if (area->fa_length > ffs_areas[best_area_idx].fa_length) {
            best_area_idx = i;
        } else if (best_area_idx == ffs_scratch_area_idx) {
            best_area_idx = i;
        } else {
            diff = ffs_areas[i].fa_gc_seq - ffs_areas[best_area_idx].fa_gc_seq;
            if (diff < 0) {
                best_area_idx = i;
            }
        }
    }

    assert(best_area_idx != ffs_scratch_area_idx);

    return best_area_idx;
}

static int
ffs_gc_block_chain_copy(struct ffs_hash_entry *last_entry, uint32_t data_len,
                        uint8_t to_area_idx)
{
    struct ffs_hash_entry *entry;
    struct ffs_block block;
    uint32_t data_bytes_copied;
    uint16_t copy_len;
    int rc;

    data_bytes_copied = 0;
    entry = last_entry;

    while (data_bytes_copied < data_len) {
        assert(entry != NULL);

        rc = ffs_block_from_hash_entry(&block, entry);
        if (rc != 0) {
            return rc;
        }

        copy_len = sizeof (struct ffs_disk_block) + block.fb_data_len;
        rc = ffs_gc_copy_object(entry, copy_len, to_area_idx);
        if (rc != 0) {
            return rc;
        }
        data_bytes_copied += block.fb_data_len;

        entry = block.fb_prev;
    }

    return 0;
}

/**
 * Moves a chain of blocks from one area to another.  This function attempts to
 * collate the blocks into a single new block in the destination area.
 *
 * @param last_entry            The last block entry in the chain.
 * @param data_len              The total length of data to collate.
 * @param to_area_idx           The index of the area to copy to.
 * @param inout_next            This parameter is only necessary if you are
 *                                  calling this function during an iteration
 *                                  of the entire hash table; pass null
 *                                  otherwise.
 *                              On input, this points to the next hash entry
 *                                  you plan on processing.
 *                              On output, this points to the next hash entry
 *                                  that should be processed.
 *
 * @return                      0 on success;
 *                              FFS_ENOMEM if there is insufficient heap;
 *                              other nonzero on failure.
 */
static int
ffs_gc_block_chain_collate(struct ffs_hash_entry *last_entry,
                           uint32_t data_len, uint8_t to_area_idx,
                           struct ffs_hash_entry **inout_next)
{
    struct ffs_disk_block disk_block;
    struct ffs_hash_entry *entry;
    struct ffs_area *to_area;
    struct ffs_block block;
    uint32_t to_area_offset;
    uint32_t from_area_offset;
    uint32_t data_offset;
    uint8_t *data;
    uint8_t from_area_idx;
    int rc;

    data = malloc(data_len);
    if (data == NULL) {
        rc = FFS_ENOMEM;
        goto done;
    }

    to_area = ffs_areas + to_area_idx;

    entry = last_entry;
    data_offset = data_len;
    while (data_offset > 0) {
        rc = ffs_block_from_hash_entry(&block, entry);
        if (rc != 0) {
            goto done;
        }
        data_offset -= block.fb_data_len;

        ffs_flash_loc_expand(block.fb_hash_entry->fhe_flash_loc,
                             &from_area_idx, &from_area_offset);
        from_area_offset += sizeof disk_block;
        rc = ffs_flash_read(from_area_idx, from_area_offset,
                            data + data_offset, block.fb_data_len);
        if (rc != 0) {
            goto done;
        }

        if (entry != last_entry) {
            if (inout_next != NULL && *inout_next == entry) {
                *inout_next = SLIST_NEXT(entry, fhe_next);
            }
            ffs_block_delete_from_ram(entry);
        }
        entry = block.fb_prev;
    }

    memset(&disk_block, 0, sizeof disk_block);
    disk_block.fdb_magic = FFS_BLOCK_MAGIC;
    disk_block.fdb_id = block.fb_hash_entry->fhe_id;
    disk_block.fdb_seq = block.fb_seq + 1;
    disk_block.fdb_inode_id = block.fb_inode_entry->fie_hash_entry.fhe_id;
    if (entry == NULL) {
        disk_block.fdb_prev_id = FFS_ID_NONE;
    } else {
        disk_block.fdb_prev_id = entry->fhe_id;
    }
    disk_block.fdb_data_len = data_len;
    ffs_crc_disk_block_fill(&disk_block, data);

    to_area_offset = to_area->fa_cur;
    rc = ffs_flash_write(to_area_idx, to_area_offset,
                         &disk_block, sizeof disk_block);
    if (rc != 0) {
        goto done;
    }

    rc = ffs_flash_write(to_area_idx, to_area_offset + sizeof disk_block,
                         data, data_len);
    if (rc != 0) {
        goto done;
    }

    last_entry->fhe_flash_loc = ffs_flash_loc(to_area_idx, to_area_offset);

    rc = 0;

    ASSERT_IF_TEST(ffs_crc_disk_block_validate(&disk_block, to_area_idx,
                                               to_area_offset) == 0);

done:
    free(data);
    return rc;
}

/**
 * Moves a chain of blocks from one area to another.  This function attempts to
 * collate the blocks into a single new block in the destination area.  If
 * there is insufficient heap memory do to this, the function falls back to
 * copying each block separately.
 *
 * @param last_entry            The last block entry in the chain.
 * @param multiple_blocks       0=single block; 1=more than one block.
 * @param data_len              The total length of data to collate.
 * @param to_area_idx           The index of the area to copy to.
 * @param inout_next            This parameter is only necessary if you are
 *                                  calling this function during an iteration
 *                                  of the entire hash table; pass null
 *                                  otherwise.
 *                              On input, this points to the next hash entry
 *                                  you plan on processing.
 *                              On output, this points to the next hash entry
 *                                  that should be processed.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_gc_block_chain(struct ffs_hash_entry *last_entry, int multiple_blocks,
                   uint32_t data_len, uint8_t to_area_idx,
                   struct ffs_hash_entry **inout_next)
{
    int rc;

    if (!multiple_blocks) {
        /* If there is only one block, collation has the same effect as a
         * simple copy.  Just perform the more efficient copy.
         */
        rc = ffs_gc_block_chain_copy(last_entry, data_len, to_area_idx);
    } else {
        rc = ffs_gc_block_chain_collate(last_entry, data_len, to_area_idx,
                                        inout_next);
        if (rc == FFS_ENOMEM) {
            /* Insufficient heap for collation; just copy each block one by
             * one.
             */
            rc = ffs_gc_block_chain_copy(last_entry, data_len, to_area_idx);
        }
    }

    return rc;
}

static int
ffs_gc_inode_blocks(struct ffs_inode_entry *inode_entry, uint8_t from_area_idx,
                    uint8_t to_area_idx, struct ffs_hash_entry **inout_next)
{
    struct ffs_hash_entry *last_entry;
    struct ffs_hash_entry *entry;
    struct ffs_block block;
    uint32_t prospective_data_len;
    uint32_t area_offset;
    uint32_t data_len;
    uint8_t area_idx;
    int multiple_blocks;
    int rc;

    assert(ffs_hash_id_is_file(inode_entry->fie_hash_entry.fhe_id));

    data_len = 0;
    last_entry = NULL;
    multiple_blocks = 0;
    entry = inode_entry->fie_last_block_entry;
    while (entry != NULL) {
        rc = ffs_block_from_hash_entry(&block, entry);
        if (rc != 0) {
            return rc;
        }

        ffs_flash_loc_expand(entry->fhe_flash_loc, &area_idx, &area_offset);
        if (area_idx == from_area_idx) {
            if (last_entry == NULL) {
                last_entry = entry;
            }

            prospective_data_len = data_len + block.fb_data_len;
            if (prospective_data_len <= ffs_block_max_data_sz) {
                data_len = prospective_data_len;
                if (last_entry != entry) {
                    multiple_blocks = 1;
                }
            } else {
                rc = ffs_gc_block_chain(last_entry, multiple_blocks, data_len,
                                        to_area_idx, inout_next);
                if (rc != 0) {
                    return rc;
                }
                last_entry = entry;
                data_len = block.fb_data_len;
                multiple_blocks = 0;
            }
        } else {
            if (last_entry != NULL) {
                rc = ffs_gc_block_chain(last_entry, multiple_blocks, data_len,
                                        to_area_idx, inout_next);
                if (rc != 0) {
                    return rc;
                }

                last_entry = NULL;
                data_len = 0;
                multiple_blocks = 0;
            }
        }

        entry = block.fb_prev;
    }

    if (last_entry != NULL) {
        rc = ffs_gc_block_chain(last_entry, multiple_blocks, data_len,
                                to_area_idx, inout_next);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

/**
 * Triggers a garbage collection cycle.  This is implemented as follows:
 *
 *  (1) The non-scratch area with the lowest garbage collection sequence
 *      number is selected as the "source area."  If there are other areas
 *      with the same sequence number, the first one encountered is selected.
 *
 *  (2) The source area's ID is written to the scratch area's header,
 *      transforming it into a non-scratch ID.  The former scratch area is now
 *      known as the "destination area."
 *
 *  (3) The RAM representation is exhaustively searched for objects which are
 *      resident in the source area.  The copy is accomplished as follows:
 *
 *      For each inode:
 *          (a) If the inode is resident in the source area, copy the inode
 *              record to the destination area.
 *
 *          (b) Walk the inode's list of data blocks, starting with the last
 *              block in the file.  Each block that is resident in the source
 *              area is copied to the destination area.  If there is a run of
 *              two or more blocks that are resident in the source area, they
 *              are consolidated and copied to the destination area as a single
 *              new block.
 *
 *  (4) The source area is reformatted as a scratch sector (i.e., its header
 *      indicates an ID of 0xffff).  The area's garbage collection sequence
 *      number is incremented prior to rewriting the header.  This area is now
 *      the new scratch sector.
 *
 * @param out_area_idx      On success, the ID of the cleaned up area gets
 *                              written here.  Pass null if you do not need
 *                              this information.
 *
 * @return                  0 on success; nonzero on error.
 */
int
ffs_gc(uint8_t *out_area_idx)
{
    struct ffs_hash_entry *entry;
    struct ffs_hash_entry *next;
    struct ffs_area *from_area;
    struct ffs_area *to_area;
    struct ffs_inode_entry *inode_entry;
    uint32_t area_offset;
    uint8_t from_area_idx;
    uint8_t area_idx;
    int rc;
    int i;

    from_area_idx = ffs_gc_select_area();
    from_area = ffs_areas + from_area_idx;
    to_area = ffs_areas + ffs_scratch_area_idx;

    rc = ffs_format_from_scratch_area(ffs_scratch_area_idx, from_area->fa_id);
    if (rc != 0) {
        return rc;
    }

    for (i = 0; i < FFS_HASH_SIZE; i++) {
        entry = SLIST_FIRST(ffs_hash + i);
        while (entry != NULL) {
            next = SLIST_NEXT(entry, fhe_next);

            if (ffs_hash_id_is_inode(entry->fhe_id)) {
                /* The inode gets copied if it is in the source area. */
                ffs_flash_loc_expand(entry->fhe_flash_loc,
                                     &area_idx, &area_offset);
                inode_entry = (struct ffs_inode_entry *)entry;
                if (area_idx == from_area_idx) {
                    rc = ffs_gc_copy_inode(inode_entry, ffs_scratch_area_idx);
                    if (rc != 0) {
                        return rc;
                    }
                }

                /* If the inode is a file, all constituent data blocks that are
                 * resident in the source area get copied.
                 */
                if (ffs_hash_id_is_file(entry->fhe_id)) {
                    rc = ffs_gc_inode_blocks(inode_entry, from_area_idx,
                                             ffs_scratch_area_idx, &next);
                    if (rc != 0) {
                        return rc;
                    }
                }
            }

            entry = next;
        }
    }

    /* The amount of written data should never increase as a result of a gc
     * cycle.
     */
    assert(to_area->fa_cur <= from_area->fa_cur);

    /* Turn the source area into the new scratch area. */
    from_area->fa_gc_seq++;
    rc = ffs_format_area(from_area_idx, 1);
    if (rc != 0) {
        return rc;
    }

    if (out_area_idx != NULL) {
        *out_area_idx = ffs_scratch_area_idx;
    }

    ffs_scratch_area_idx = from_area_idx;

    return 0;
}

/**
 * Repeatedly performs garbage collection cycles until there is enough free
 * space to accommodate an object of the specified size.  If there still isn't
 * enough free space after every area has been garbage collected, this function
 * fails.
 *
 * @param space                 The number of bytes of free space required.
 * @param out_area_idx          On success, the index of the area which can
 *                                  accommodate the necessary data.
 *
 * @return                      0 on success;
 *                              FFS_EFULL if the necessary space could not be
 *                                  freed.
 *                              nonzero on other failure.
 */
int
ffs_gc_until(uint32_t space, uint8_t *out_area_idx)
{
    int rc;
    int i;

    for (i = 0; i < ffs_num_areas; i++) {
        rc = ffs_gc(out_area_idx);
        if (rc != 0) {
            return rc;
        }

        if (ffs_area_free_space(ffs_areas + *out_area_idx) >= space) {
            return 0;
        }
    }

    return FFS_EFULL;
}
