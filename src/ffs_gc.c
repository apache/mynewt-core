#include <assert.h>
#include <string.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"


/**
 * Calculates the amount of disk space required by the specified object.
 */
static uint32_t
ffs_gc_base_disk_size(const struct ffs_base *base)
{
    const struct ffs_inode *inode;
    const struct ffs_block *block;

    switch (base->fb_type) {
    case FFS_OBJECT_TYPE_INODE:
        inode = (void *)base;
        return sizeof (struct ffs_disk_inode) + inode->fi_filename_len;

    case FFS_OBJECT_TYPE_BLOCK:
        block = (void *)base;
        return sizeof (struct ffs_disk_block) + block->fb_data_len;

    default:
        assert(0);
        break;
    }
}

/**
 * Selects the most appropriate sector for garbage collection.
 *
 * @return                  The ID of the sector to garbage collect.
 */
static uint16_t
ffs_gc_select_sector(void)
{
    const struct ffs_sector *sector;
    uint16_t best_sector_id;
    int8_t diff;
    int i;

    best_sector_id = 0;
    for (i = 1; i < ffs_num_sectors; i++) {
        if (i == ffs_scratch_sector_id) {
            continue;
        }

        sector = ffs_sectors + i;
        if (sector->fs_length > ffs_sectors[best_sector_id].fs_length) {
            best_sector_id = i;
        } else if (best_sector_id == ffs_scratch_sector_id) {
            best_sector_id = i;
        } else {
            diff = ffs_sectors[i].fs_seq - ffs_sectors[best_sector_id].fs_seq;
            if (diff < 0) {
                best_sector_id = i;
            }
        }
    }

    assert(best_sector_id != ffs_scratch_sector_id);

    return best_sector_id;
}

static int
ffs_gc_block_chain(struct ffs_block *first_block, struct ffs_block *last_block,
                   uint32_t data_len, uint16_t to_sector_id)
{
    struct ffs_disk_block disk_block;
    struct ffs_sector *to_sector;
    struct ffs_block *block;
    struct ffs_block *next;
    struct ffs_inode *inode;
    uint32_t to_offset;
    int rc;

    to_sector = ffs_sectors + to_sector_id;

    memset(&disk_block, 0, sizeof disk_block);
    disk_block.fdb_magic = FFS_BLOCK_MAGIC;
    disk_block.fdb_id = first_block->fb_base.fb_id;
    disk_block.fdb_seq = first_block->fb_base.fb_seq + 1;
    disk_block.fdb_rank = first_block->fb_rank;
    disk_block.fdb_inode_id = first_block->fb_inode->fi_base.fb_id;
    disk_block.fdb_flags = first_block->fb_flags;
    disk_block.fdb_data_len = data_len;

    to_offset = to_sector->fs_cur;
    rc = ffs_flash_write(to_sector_id, to_offset,
                         &disk_block, sizeof disk_block);
    if (rc != 0) {
        return rc;
    }

    inode = first_block->fb_inode;

    block = first_block;
    while (1) {
        rc = ffs_flash_copy(block->fb_base.fb_sector_id,
                            block->fb_base.fb_offset + sizeof disk_block,
                            to_sector_id, to_sector->fs_cur,
                            block->fb_data_len);
        if (rc != 0) {
            return rc;
        }

        block->fb_base.fb_sector_id = to_sector_id;
        block->fb_base.fb_offset = to_offset;

        next = SLIST_NEXT(block, fb_next);
        if (block != first_block) {
            block->fb_data_len = 0;
            ffs_block_delete_from_ram(block);
        }
        if (block == last_block) {
            break;
        }

        block = next;
    }

    first_block->fb_data_len = data_len;

    assert(inode->fi_data_len == ffs_inode_calc_data_length(inode));

    return 0;
}

static int
ffs_gc_inode_blocks(struct ffs_inode *inode, uint16_t from_sector_id,
                    uint16_t to_sector_id)
{
    struct ffs_block *first_block;
    struct ffs_block *prev_block;
    struct ffs_block *block;
    uint32_t data_len;
    uint32_t perspective_data_len;
    int rc;

    assert(!(inode->fi_flags & FFS_INODE_F_DIRECTORY));

    first_block = NULL;
    prev_block = NULL;
    data_len = 0;
    SLIST_FOREACH(block, &inode->fi_block_list, fb_next) {
        if (block->fb_base.fb_sector_id == from_sector_id) {
            if (first_block == NULL) {
                first_block = block;
            }

            perspective_data_len = data_len + block->fb_data_len;
            if (perspective_data_len <= FFS_BLOCK_MAX_DATA_SZ) {
                data_len = perspective_data_len;
            } else {
                rc = ffs_gc_block_chain(first_block, prev_block, data_len,
                                        to_sector_id);
                if (rc != 0) {
                    return rc;
                }
                data_len = block->fb_data_len;
            }
        } else {
            if (first_block != NULL) {
                rc = ffs_gc_block_chain(first_block, prev_block, data_len,
                                        to_sector_id);
                if (rc != 0) {
                    return rc;
                }

                first_block = NULL;
                data_len = 0;
            }
        }

        prev_block = block;
    }

    if (first_block != NULL) {
        rc = ffs_gc_block_chain(first_block, prev_block, data_len,
                                to_sector_id);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

/**
 * Triggers a garbage collection cycle.
 *
 * @param out_sector_id     On success, the ID of the cleaned up sector gets
 *                          written here.  Pass null if you do not need this
 *                          information.
 *
 * @return                  0 on success; nonzero on error.
 */
int
ffs_gc(uint16_t *out_sector_id)
{
    struct ffs_sector *from_sector;
    struct ffs_sector *to_sector;
    struct ffs_inode *inode;
    struct ffs_base *base;
    uint32_t to_offset;
    uint32_t obj_size;
    uint16_t from_sector_id;
    int rc;
    int i;

    rc = ffs_format_from_scratch_sector(ffs_scratch_sector_id);
    if (rc != 0) {
        return rc;
    }

    from_sector_id = ffs_gc_select_sector();
    FFS_HASH_FOREACH(base, i) {
        if (base->fb_type == FFS_OBJECT_TYPE_INODE) {
            inode = (struct ffs_inode *)base;
            if (!(inode->fi_flags & FFS_INODE_F_DIRECTORY)) {
                rc = ffs_gc_inode_blocks(inode, from_sector_id,
                                         ffs_scratch_sector_id);
                if (rc != 0) {
                    return rc;
                }
            }
        }
    }

    to_sector = ffs_sectors + ffs_scratch_sector_id;
    FFS_HASH_FOREACH(base, i) {
        if (base->fb_sector_id == from_sector_id) {
            obj_size = ffs_gc_base_disk_size(base);
            to_offset = to_sector->fs_cur;
            rc = ffs_flash_copy(from_sector_id, base->fb_offset,
                                ffs_scratch_sector_id, to_offset,
                                obj_size);
            if (rc != 0) {
                return rc;
            }
            base->fb_sector_id = ffs_scratch_sector_id;
            base->fb_offset = to_offset;
        }
    }

    from_sector = ffs_sectors + from_sector_id;
    from_sector->fs_seq++;
    rc = ffs_format_sector(from_sector_id, 1);
    if (rc != 0) {
        return rc;
    }

    if (out_sector_id != NULL) {
        *out_sector_id = ffs_scratch_sector_id;
    }

    ffs_scratch_sector_id = from_sector_id;

    return 0;
}

int
ffs_gc_until(uint16_t *out_sector_id, uint32_t space)
{
    int rc;
    int i;

    for (i = 0; i < ffs_num_sectors; i++) {
        rc = ffs_gc(out_sector_id);
        if (rc != 0) {
            return rc;
        }

        if (ffs_sector_free_space(ffs_sectors + *out_sector_id) >= space) {
            return 0;
        }
    }

    return FFS_EFULL;
}

