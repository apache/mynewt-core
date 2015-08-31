#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hal/hal_flash.h"
#include "os/os_mempool.h"
#include "os/os_malloc.h"
#include "ffs/ffs.h"
#include "ffs_priv.h"

/**
 * The size of the largest data block encountered during detection.  This is
 * used to ensure that the maximum block data size is not set lower than the
 * size of an existing block.
 */
static uint16_t ffs_restore_largest_block_data_len;

/**
 * Checks the CRC of each block in a chain of data blocks.
 *
 * @param last_block_entry      The entry corresponding to the last block in
 *                                  the chain.
 *
 * @return                      0 if the block chain is OK;
 *                              FFS_ECORRUPT if corruption is detected;
 *                              nonzero on other error.
 */
static int
ffs_restore_validate_block_chain(struct ffs_hash_entry *last_block_entry)
{
    struct ffs_disk_block disk_block;
    struct ffs_hash_entry *cur;
    struct ffs_block block;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    cur = last_block_entry;

    while (cur != NULL) {
        ffs_flash_loc_expand(cur->fhe_flash_loc, &area_idx, &area_offset);

        rc = ffs_block_read_disk(area_idx, area_offset, &disk_block);
        if (rc != 0) {
            return rc;
        }

        rc = ffs_crc_disk_block_validate(&disk_block, area_idx, area_offset);
        if (rc != 0) {
            return rc;
        }

        rc = ffs_block_from_hash_entry(&block, cur);
        if (rc != 0) {
            return rc;
        }

        cur = block.fb_prev;
    }

    return 0;
}

/**
 * If the specified inode entry is a dummy directory, this function moves
 * all its children to the lost+found directory.
 *
 * @param inode_entry           The parent inode to test and empty.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_migrate_orphan_children(struct ffs_inode_entry *inode_entry)
{
    struct ffs_inode_entry *lost_found_sub;
    struct ffs_inode_entry *child_entry;
    //struct ffs_inode child_inode;
    char buf[32];
    int rc;

    if (!ffs_hash_id_is_dir(inode_entry->fie_hash_entry.fhe_id)) {
        /* Not a directory. */
        return 0;
    }

    if (inode_entry->fie_refcnt != 0) {
        /* Not a dummy. */
        return 0;
    }

    if (SLIST_EMPTY(&inode_entry->fie_child_list)) {
        /* No children to migrate. */
        return 0;
    }

    /* Create a directory in lost+found to hold the dummy directory's
     * contents.
     */
    snprintf(buf, sizeof buf, "/lost+found/%lu",
             (unsigned long)inode_entry->fie_hash_entry.fhe_id);
    rc = ffs_path_new_dir(buf, &lost_found_sub);
    if (rc != 0 && rc != FFS_EEXIST) {
        return rc;
    }

    /* Move each child into the new subdirectory. */
    while ((child_entry = SLIST_FIRST(&inode_entry->fie_child_list)) != NULL) {
        rc = ffs_inode_rename(child_entry, lost_found_sub, NULL);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

static int
ffs_restore_should_sweep_inode_entry(struct ffs_inode_entry *inode_entry,
                                     int *out_should_sweep)
{
    struct ffs_inode inode;
    int rc;

    /* Determine if the inode is a dummy.  Dummy inodes have a reference count
     * of 0.  If it is a dummy, increment its reference count back to 1 so that
     * it can be properly deleted.  The presence of a dummy inode during the
     * final sweep step indicates file system corruption.  If the inode is a
     * directory, all its children should have been migrated to the /lost+found
     * directory prior to this.
     */
    if (inode_entry->fie_refcnt == 0) {
        *out_should_sweep = 1;
        inode_entry->fie_refcnt++;
        return 0;
    }

    /* Determine if the inode has been deleted.  If an inode has no parent (and
     * it isn't the root directory), it has been deleted from the disk and
     * should be swept from the RAM representation.
     */
    if (inode_entry->fie_hash_entry.fhe_id != FFS_ID_ROOT_DIR) {
        rc = ffs_inode_from_entry(&inode, inode_entry);
        if (rc != 0) {
            *out_should_sweep = 0;
            return rc;
        }

        if (inode.fi_parent == NULL) {
            *out_should_sweep = 1;
            return 0;
        }
    }

    /* If this is a file inode, verify that none of its constituent blocks are
     * corrupt via a CRC check.
     */
    if (ffs_hash_id_is_file(inode_entry->fie_hash_entry.fhe_id)) {
        rc = ffs_restore_validate_block_chain(
                inode_entry->fie_last_block_entry);
        if (rc == FFS_ECORRUPT) {
            *out_should_sweep = 1;
            return 0;
        } else if (rc != 0) {
            *out_should_sweep = 0;
            return rc;
        }
    }

    /* This is a valid inode; don't sweep it. */
    *out_should_sweep = 0;
    return 0;
}

static void
ffs_restore_inode_from_dummy_entry(struct ffs_inode *out_inode,
                                   struct ffs_inode_entry *inode_entry)
{
    memset(out_inode, 0, sizeof *out_inode);
    out_inode->fi_inode_entry = inode_entry;
}

/**
 * Performs a sweep of the RAM representation at the end of a successful
 * restore.  The sweep phase performs the following actions of each inode in
 * the file system:
 *     1. If the inode is a dummy directory, its children are migrated to the
 *        lost+found directory.
 *     2. Else if the inode is a dummy file, it is fully deleted from RAM.
 *     3. Else, a CRC check is performed on each of the inode's constituent
 *        blocks.  If corruption is detected, the inode is fully deleted from
 *        RAM.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_restore_sweep(void)
{
    struct ffs_inode_entry *inode_entry;
    struct ffs_hash_entry *entry;
    struct ffs_hash_entry *next;
    struct ffs_hash_list *list;
    struct ffs_inode inode;
    int del;
    int rc;
    int i;

    /* Iterate through every object in the hash table, deleting all inodes that
     * should be removed.
     */
    for (i = 0; i < FFS_HASH_SIZE; i++) {
        list = ffs_hash + i;

        entry = SLIST_FIRST(list);
        while (entry != NULL) {
            next = SLIST_NEXT(entry, fhe_next);
            if (ffs_hash_id_is_inode(entry->fhe_id)) {
                inode_entry = (struct ffs_inode_entry *)entry;

                /* If this is a dummy inode directory, the file system is
                 * corrupted.  Move the directory's children inodes to the
                 * lost+found directory.
                 */
                rc = ffs_restore_migrate_orphan_children(inode_entry);
                if (rc != 0) {
                    return rc;
                }

                /* Determine if this inode needs to be deleted. */
                rc = ffs_restore_should_sweep_inode_entry(inode_entry, &del);
                if (rc != 0) {
                    return rc;
                }

                if (del) {
                    if (inode_entry->fie_hash_entry.fhe_flash_loc ==
                        FFS_FLASH_LOC_NONE) {

                        ffs_restore_inode_from_dummy_entry(&inode,
                                                           inode_entry);
                    } else {
                        rc = ffs_inode_from_entry(&inode, inode_entry);
                        if (rc != 0) {
                            return rc;
                        }
                    }

                    /* Remove the inode and all its children from RAM. */
                    rc = ffs_inode_unlink_from_ram(&inode, &next);
                    if (rc != 0) {
                        return rc;
                    }
                    next = SLIST_FIRST(list);
                }
            }

            entry = next;
        }
    }

    return 0;
}

/**
 * Creates a dummy inode and inserts it into the hash table.  A dummy inode is
 * a temporary placeholder for a real inode that has not been restored yet.
 * These are necessary so that the inter-object links can be maintained until
 * the absent inode is eventually restored.  Dummy inodes are identified by a
 * reference count of 0.
 *
 * @param id                    The ID of the dummy inode to create.
 * @param out_inode_entry       On success, the dummy inode gets written here.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_dummy_inode(uint32_t id, struct ffs_inode_entry **out_inode_entry)
{
    struct ffs_inode_entry *inode_entry;

    inode_entry = ffs_inode_entry_alloc();
    if (inode_entry == NULL) {
        return FFS_ENOMEM;
    }
    inode_entry->fie_hash_entry.fhe_id = id;
    inode_entry->fie_hash_entry.fhe_flash_loc = FFS_FLASH_LOC_NONE;
    inode_entry->fie_refcnt = 0;

    ffs_hash_insert(&inode_entry->fie_hash_entry);

    *out_inode_entry = inode_entry;

    return 0;
}

/**
 * Determines if an already-restored inode should be replaced by another inode
 * just read from flash.  This function should only be called if both inodes
 * share the same ID.  The existing inode gets replaced if:
 *     o It is a dummy inode.
 *     o Its sequence number is less than that of the new inode.
 *
 * @param old_inode_entry       The already-restored inode to test.
 * @param disk_inode            The inode just read from flash.
 * @param out_should_replace    On success, 0=don't replace; 1=do replace.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_inode_gets_replaced(struct ffs_inode_entry *old_inode_entry,
                                const struct ffs_disk_inode *disk_inode,
                                int *out_should_replace)
{
    struct ffs_inode old_inode;
    int rc;

    assert(old_inode_entry->fie_hash_entry.fhe_id == disk_inode->fdi_id);

    if (old_inode_entry->fie_refcnt == 0) {
        *out_should_replace = 1;
        return 0;
    }

    rc = ffs_inode_from_entry(&old_inode, old_inode_entry);
    if (rc != 0) {
        *out_should_replace = 0;
        return rc;
    }

    if (old_inode.fi_seq < disk_inode->fdi_seq) {
        *out_should_replace = 1;
        return 0;
    }

    if (old_inode.fi_seq == disk_inode->fdi_seq) {
        /* This is a duplicate of a previously-read inode.  This should never
         * happen.
         */
        *out_should_replace = 0;
        return FFS_ECORRUPT;
    }

    *out_should_replace = 0;
    return 0;
}

/**
 * Determines if the specified inode should be added to the RAM representation
 * and adds it if appropriate.
 *
 * @param disk_inode            The inode just read from flash.
 * @param area_idx              The index of the area containing the inode.
 * @param area_offset           The offset within the area of the inode.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_inode(const struct ffs_disk_inode *disk_inode, uint8_t area_idx,
                  uint32_t area_offset)
{
    struct ffs_inode_entry *inode_entry;
    struct ffs_inode_entry *parent;
    struct ffs_inode inode;
    int new_inode;
    int do_add;
    int rc;

    new_inode = 0;

    /* Check the inode's CRC.  If the inode is corrupt, mark it as a dummy
     * node.  If the corrupt inode does not get superseded by a valid revision,
     * it will get deleted during the sweep phase.
     */
    rc = ffs_crc_disk_inode_validate(disk_inode, area_idx, area_offset);
    if (rc != 0) {
        goto err;
    }

    inode_entry = ffs_hash_find_inode(disk_inode->fdi_id);
    if (inode_entry != NULL) {
        rc = ffs_restore_inode_gets_replaced(inode_entry, disk_inode, &do_add);
        if (rc != 0) {
            goto err;
        }

        if (do_add) {
            if (inode_entry->fie_hash_entry.fhe_flash_loc !=
                FFS_FLASH_LOC_NONE) {

                rc = ffs_inode_from_entry(&inode, inode_entry);
                if (rc != 0) {
                    return rc;
                }
                if (inode.fi_parent != NULL) {
                    ffs_inode_remove_child(&inode);
                }
            }
 
            inode_entry->fie_hash_entry.fhe_flash_loc =
                ffs_flash_loc(area_idx, area_offset);
        }
    } else {
        inode_entry = ffs_inode_entry_alloc();
        if (inode_entry == NULL) {
            rc = FFS_ENOMEM;
            goto err;
        }
        new_inode = 1;
        do_add = 1;

        inode_entry->fie_hash_entry.fhe_id = disk_inode->fdi_id;
        inode_entry->fie_hash_entry.fhe_flash_loc =
            ffs_flash_loc(area_idx, area_offset);

        ffs_hash_insert(&inode_entry->fie_hash_entry);
    }

    if (do_add) {
        inode_entry->fie_refcnt = 1;

        if (disk_inode->fdi_parent_id != FFS_ID_NONE) {
            parent = ffs_hash_find_inode(disk_inode->fdi_parent_id);
            if (parent == NULL) {
                rc = ffs_restore_dummy_inode(disk_inode->fdi_parent_id,
                                             &parent);
                if (rc != 0) {
                    goto err;
                }
            }

            rc = ffs_inode_add_child(parent, inode_entry);
            if (rc != 0) {
                goto err;
            }
        } 


        if (inode_entry->fie_hash_entry.fhe_id == FFS_ID_ROOT_DIR) {
            ffs_root_dir = inode_entry;
        }
    }

    if (ffs_hash_id_is_file(inode_entry->fie_hash_entry.fhe_id)) {
        if (inode_entry->fie_hash_entry.fhe_id >= ffs_hash_next_file_id) {
            ffs_hash_next_file_id = inode_entry->fie_hash_entry.fhe_id + 1;
        }
    } else {
        if (inode_entry->fie_hash_entry.fhe_id >= ffs_hash_next_dir_id) {
            ffs_hash_next_dir_id = inode_entry->fie_hash_entry.fhe_id + 1;
        }
    }

    return 0;

err:
    if (new_inode) {
        ffs_inode_entry_free(inode_entry);
    }
    return rc;
}

/**
 * Indicates whether the specified data block is superseded by the just-read
 * disk data block.  A data block supersedes another if its ID is equal and its
 * sequence number is greater than that of the other block.
 *
 * @param out_should_replace    On success, 0 or 1 gets written here, to
 *                                  indicate whether replacement should occur.
 * @param old_block             The data block which has already been read and
 *                                  converted to its RAM representation.  This
 *                                  is the block that may be superseded.
 * @param disk_block            The disk data block that was just read from
 *                                  flash.  This is the block which may
 *                                  supersede the other.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_block_gets_replaced(const struct ffs_block *old_block,
                                const struct ffs_disk_block *disk_block,
                                int *out_should_replace)
{
    assert(old_block->fb_hash_entry->fhe_id == disk_block->fdb_id);

    if (old_block->fb_seq < disk_block->fdb_seq) {
        *out_should_replace = 1;
        return 0;
    }

    if (old_block->fb_seq == disk_block->fdb_seq) {
        /* This is a duplicate of an previously-read inode.  This should never
         * happen.
         */
        return FFS_ECORRUPT;
    }

    *out_should_replace = 0;
    return 0;
}

/**
 * Populates the ffs RAM state with the memory representation of the specified
 * disk data block.
 *
 * @param disk_block            The source disk block to insert.
 * @param area_idx              The ID of the area containing the block.
 * @param area_offset           The area_offset within the area of the block.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_block(const struct ffs_disk_block *disk_block, uint8_t area_idx,
                  uint32_t area_offset)
{
    struct ffs_inode_entry *inode_entry;
    struct ffs_hash_entry *entry;
    struct ffs_block block;
    int do_replace;
    int new_block;
    int rc;

    new_block = 0;

    /* Check the block's CRC.  If the block is corrupt, discard it.  If this
     * block would have superseded another, the old block becomes current.
     */
    rc = ffs_crc_disk_block_validate(disk_block, area_idx, area_offset);
    if (rc != 0) {
        goto err;
    }

    entry = ffs_hash_find_block(disk_block->fdb_id);
    if (entry != NULL) {
        rc = ffs_block_from_hash_entry_no_ptrs(&block, entry);
        if (rc != 0) {
            goto err;
        }

        rc = ffs_restore_block_gets_replaced(&block, disk_block, &do_replace);
        if (rc != 0) {
            goto err;
        }

        if (!do_replace) {
            /* The new block is superseded by the old; nothing to do. */
            return 0;
        }

        ffs_block_delete_from_ram(entry);
    }

    entry = ffs_block_entry_alloc();
    if (entry == NULL) {
        rc = FFS_ENOMEM;
        goto err;
    }
    new_block = 1;
    entry->fhe_id = disk_block->fdb_id;
    entry->fhe_flash_loc = ffs_flash_loc(area_idx, area_offset);

    /* The block is ready to be inserted into the hash. */

    inode_entry = ffs_hash_find_inode(disk_block->fdb_inode_id);
    if (inode_entry == NULL) {
        rc = ffs_restore_dummy_inode(disk_block->fdb_inode_id, &inode_entry);
        if (rc != 0) {
            goto err;
        }
    }

    if (inode_entry->fie_last_block_entry == NULL ||
        inode_entry->fie_last_block_entry->fhe_id == disk_block->fdb_prev_id) {

        inode_entry->fie_last_block_entry = entry;
    }

    ffs_hash_insert(entry);

    if (disk_block->fdb_id >= ffs_hash_next_block_id) {
        ffs_hash_next_block_id = disk_block->fdb_id + 1;
    }

    /* Make sure the maximum block data size is not set lower than the size of
     * an existing block.
     */
    if (disk_block->fdb_data_len > ffs_restore_largest_block_data_len) {
        ffs_restore_largest_block_data_len = disk_block->fdb_data_len;
    }

    return 0;

err:
    if (new_block) {
        ffs_block_entry_free(entry);
    }
    return rc;
}

/**
 * Populates the ffs RAM state with the memory representation of the specified
 * disk object.
 *
 * @param disk_object           The source disk object to convert.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_object(const struct ffs_disk_object *disk_object)
{
    int rc;

    switch (disk_object->fdo_type) {
    case FFS_OBJECT_TYPE_INODE:
        rc = ffs_restore_inode(&disk_object->fdo_disk_inode,
                               disk_object->fdo_area_idx,
                               disk_object->fdo_offset);
        break;

    case FFS_OBJECT_TYPE_BLOCK:
        rc = ffs_restore_block(&disk_object->fdo_disk_block,
                               disk_object->fdo_area_idx,
                               disk_object->fdo_offset);
        break;

    default:
        assert(0);
        rc = FFS_EINVAL;
        break;
    }

    return rc;
}

/**
 * Reads a single disk object from flash.
 *
 * @param area_idx              The area to read the object from.
 * @param area_offset           The offset within the area to read from.
 * @param out_disk_object       On success, the restored object gets written
 *                                  here.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_disk_object(int area_idx, uint32_t area_offset,
                        struct ffs_disk_object *out_disk_object)
{
    uint32_t magic;
    int rc;

    rc = ffs_flash_read(area_idx, area_offset, &magic, sizeof magic);
    if (rc != 0) {
        return rc;
    }

    switch (magic) {
    case FFS_INODE_MAGIC:
        out_disk_object->fdo_type = FFS_OBJECT_TYPE_INODE;
        rc = ffs_inode_read_disk(area_idx, area_offset,
                                 &out_disk_object->fdo_disk_inode);
        break;

    case FFS_BLOCK_MAGIC:
        out_disk_object->fdo_type = FFS_OBJECT_TYPE_BLOCK;
        rc = ffs_block_read_disk(area_idx, area_offset,
                                 &out_disk_object->fdo_disk_block);
        break;

    case 0xffffffff:
        rc = FFS_EEMPTY;
        break;

    default:
        rc = FFS_ECORRUPT;
        break;
    }

    if (rc != 0) {
        return rc;
    }

    out_disk_object->fdo_area_idx = area_idx;
    out_disk_object->fdo_offset = area_offset;

    return 0;
}

/**
 * Calculates the disk space occupied by the specified disk object.
 *
 * @param disk_object
 */
static int
ffs_restore_disk_object_size(const struct ffs_disk_object *disk_object)
{
    switch (disk_object->fdo_type) {
    case FFS_OBJECT_TYPE_INODE:
        return sizeof disk_object->fdo_disk_inode +
                      disk_object->fdo_disk_inode.fdi_filename_len;

    case FFS_OBJECT_TYPE_BLOCK:
        return sizeof disk_object->fdo_disk_block +
                      disk_object->fdo_disk_block.fdb_data_len;

    default:
        assert(0);
        return 1;
    }
}

/**
 * Reads the specified area from disk and loads its contents into the RAM
 * representation.
 *
 * @param area_idx              The index of the area to read.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_area_contents(int area_idx)
{
    struct ffs_disk_object disk_object;
    struct ffs_disk_area disk_area;
    struct ffs_area *area;
    int rc;

    area = ffs_areas + area_idx;

    area->fa_cur = sizeof disk_area;
    while (1) {
        rc = ffs_restore_disk_object(area_idx, area->fa_cur, &disk_object);
        switch (rc) {
        case 0:
            ffs_restore_object(&disk_object);
            area->fa_cur += ffs_restore_disk_object_size(&disk_object);
            break;

        case FFS_EEMPTY:
        case FFS_ERANGE:
            return 0;

        default:
            return rc;
        }
    }
}

/**
 * Reads and parses one area header.  This function does not read the area's
 * contents.
 *
 * @param out_is_scratch        On success, 0 or 1 gets written here,
 *                                  indicating whether the area is a scratch
 *                                  area.
 * @param area_offset           The flash offset of the start of the area.
 *
 * @return                      0 on success;
 *                              nonzero on failure.
 */
static int
ffs_restore_detect_one_area(uint32_t area_offset,
                            struct ffs_disk_area *out_disk_area)
{
    int rc;

    rc = flash_read(area_offset, out_disk_area, sizeof *out_disk_area);
    if (rc != 0) {
        return FFS_EFLASH_ERROR;
    }

    if (!ffs_area_magic_is_set(out_disk_area)) {
        return FFS_ECORRUPT;
    }

    return 0;
}

/**
 * Repairs the effects of a corrupt scratch area.  Scratch area corruption can
 * occur when the system resets while a garbage collection cycle is in
 * progress.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
ffs_restore_corrupt_scratch(void)
{
    struct ffs_inode_entry *inode_entry;
    struct ffs_hash_entry *entry;
    struct ffs_hash_entry *next;
    uint32_t area_offset;
    uint16_t good_idx;
    uint16_t bad_idx;
    uint8_t area_idx;
    int rc;
    int i;

    /* Search for a pair of areas with identical IDs.  If found, these areas
     * represent the source and destination areas of a garbage collection
     * cycle.  The shorter of the two areas was the destination area.  Since
     * the garbage collection cycle did not finish, the source area contains a
     * more complete set of objects than the destination area.
     *
     * good_idx = index of source area.
     * bad_idx  = index of destination area; this will be turned into the
     *            scratch area.
     */
    rc = ffs_area_find_corrupt_scratch(&good_idx, &bad_idx);
    if (rc != 0) {
        return rc;
    }

    /* Invalidate all objects resident in the bad area. */
    for (i = 0; i < FFS_HASH_SIZE; i++) {
        entry = SLIST_FIRST(&ffs_hash[i]);
        while (entry != NULL) {
            next = SLIST_NEXT(entry, fhe_next);

            ffs_flash_loc_expand(entry->fhe_flash_loc,
                                 &area_idx, &area_offset);
            if (area_idx == bad_idx) {
                if (ffs_hash_id_is_block(entry->fhe_id)) {
                    rc = ffs_block_delete_from_ram(entry);
                    if (rc != 0) {
                        return rc;
                    }
                } else {
                    inode_entry = (struct ffs_inode_entry *)entry;
                    inode_entry->fie_refcnt = 0;
                }
            }

            entry = next;
        }
    }

    /* Now that the objects in the scratch area have been invalidated, reload
     * everything from the good area.
     */
    rc = ffs_restore_area_contents(good_idx);
    if (rc != 0) {
        return rc;
    }

    /* Convert the bad area into a scratch area. */
    rc = ffs_format_area(bad_idx, 1);
    if (rc != 0) {
        return rc;
    }
    ffs_scratch_area_idx = bad_idx;

    return 0;
}

/**
 * Searches for a valid ffs file system among the specified areas.  This
 * function succeeds if a file system is detected among any subset of the
 * supplied areas.  If the area set does not contain a valid file system,
 * a new one can be created via a call to ffs_format().
 *
 * @param area_descs        The area set to search.  This array must be
 *                              terminated with a 0-length area.
 *
 * @return                  0 on success;
 *                          FFS_ECORRUPT if no valid file system was detected;
 *                          other nonzero on error.
 */
int
ffs_restore_full(const struct ffs_area_desc *area_descs)
{
    struct ffs_disk_area disk_area;
    int cur_area_idx;
    int use_area;
    int rc;
    int i;

    /* Start from a clean state. */
    ffs_misc_reset();
    ffs_restore_largest_block_data_len = 0;

    /* Read each area from flash. */
    for (i = 0; area_descs[i].fad_length != 0; i++) {
        if (i > FFS_MAX_AREAS) {
            rc = FFS_EINVAL;
            goto err;
        }

        rc = ffs_restore_detect_one_area(area_descs[i].fad_offset, &disk_area);
        switch (rc) {
        case 0:
            use_area = 1;
            break;

        case FFS_ECORRUPT:
            use_area = 0;
            break;

        default:
            goto err;
        }

        if (use_area) {
            if (disk_area.fda_id == FFS_AREA_ID_NONE &&
                ffs_scratch_area_idx != FFS_AREA_ID_NONE) {

                /* Don't allow more than one scratch area. */
                use_area = 0;
            }
        }

        if (use_area) {
            /* Populate RAM with a representation of this area. */
            cur_area_idx = ffs_num_areas;

            rc = ffs_misc_set_num_areas(ffs_num_areas + 1);
            if (rc != 0) {
                goto err;
            }

            ffs_areas[cur_area_idx].fa_offset = area_descs[i].fad_offset;
            ffs_areas[cur_area_idx].fa_length = area_descs[i].fad_length;
            ffs_areas[cur_area_idx].fa_gc_seq = disk_area.fda_gc_seq;
            ffs_areas[cur_area_idx].fa_id = disk_area.fda_id;

            if (disk_area.fda_id == FFS_AREA_ID_NONE) {
                ffs_areas[cur_area_idx].fa_cur = FFS_AREA_OFFSET_ID;
                ffs_scratch_area_idx = cur_area_idx;
            } else {
                ffs_areas[cur_area_idx].fa_cur = sizeof (struct ffs_disk_area);
                ffs_restore_area_contents(cur_area_idx);
            }
        }
    }

    /* All areas have been restored from flash. */

    if (ffs_scratch_area_idx == FFS_AREA_ID_NONE) {
        /* No scratch area.  The system may have been rebooted in the middle of
         * a garbage collection cycle.  Look for a candidate scratch area.
         */
        rc = ffs_restore_corrupt_scratch();
        if (rc != 0) {
            goto err;
        }
    }

    /* Ensure this file system contains a valid scratch area. */
    rc = ffs_misc_validate_scratch();
    if (rc != 0) {
        goto err;
    }

    /* Ensure there is a "/lost+found" directory. */
    rc = ffs_misc_create_lost_found_dir();
    if (rc != 0) {
        goto err;
    }

    /* Delete from RAM any objects that were invalidated when subsequent areas
     * were restored.
     */
    ffs_restore_sweep();

    /* Make sure the file system contains a valid root directory. */
    rc = ffs_misc_validate_root_dir();
    if (rc != 0) {
        goto err;
    }

    /* Set the maximum data block size according to the size of the smallest
     * area.
     * XXX Don't set size less than largest existing block (in case set of
     * areas was changed).
     */
    rc = ffs_misc_set_max_block_data_len(ffs_restore_largest_block_data_len);
    if (rc != 0) {
        goto err;
    }

    return 0;

err:
    ffs_misc_reset();
    return rc;
}
