#include <assert.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

static uint16_t
ffs_gc_select_sector(void)
{
    const struct ffs_base *base;
    uint32_t best_trash;
    uint32_t trash;
    uint16_t best_sector_id;
    int i;

    best_sector_id = 0;
    best_trash = 0;
    for (i = 0; i < ffs_num_sectors; i++) {
        ffs_sectors[i].fsi_trash = 0;
    }

    FFS_HASH_FOREACH(base) {
        ffs_sectors[i].fsi_trash += ffs_base_disk_size(base);
    }

    for (i = 0; i < ffs_num_sectors; i++) {
        if (i != ffs_scratch_sector_id) {
            assert(ffs_sectors[i].fsi_trash <= ffs_sectors[i].fsi_length);
            trash = ffs_sectors[i].fsi_length - ffs_sectors[i].fsi_trash;
            if (trash > best_trash) {
                best_sector_id = i;
                best_trash = trash;
            }
        }
    }

    return best_sector_id;
}

int
ffs_gc(uint16_t *out_sector_id)
{
    struct ffs_sector_info *to_sector;
    struct ffs_base *base;
    uint16_t from_sector_id;
    uint32_t obj_size;
    uint32_t to_offset;
    int rc;
    int i;

    rc = ffs_format_from_scratch_sector(ffs_scratch_sector_id);
    if (rc != 0) {
        return rc;
    }

    from_sector_id = ffs_gc_select_sector();
    to_sector = ffs_sectors + ffs_scratch_sector_id;

    FFS_HASH_FOREACH(base) {
        if (base->fb_sector_id == from_sector_id) {
            obj_size = ffs_base_disk_size(base);
            to_offset = to_sector->fsi_cur;
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

    rc = ffs_format_scratch_sector(from_sector_id);
    if (rc != 0) {
        return FFS_EFLASH_ERROR;
    }

    *out_sector_id = ffs_scratch_sector_id;
    ffs_scratch_sector_id = from_sector_id;

    return 0;
}

