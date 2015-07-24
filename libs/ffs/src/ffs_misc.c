#include <assert.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

int
ffs_misc_validate_root(void)
{
    if (ffs_root_dir == NULL) {
        return FFS_ECORRUPT;
    }

    return 0;
}

int
ffs_misc_validate_scratch(void)
{
    uint32_t scratch_len;
    int i;

    if (ffs_scratch_sector_id == FFS_SECTOR_ID_SCRATCH) {
        /* No scratch sector. */
        return FFS_ECORRUPT;
    }

    scratch_len = ffs_sectors[ffs_scratch_sector_id].fs_length;
    for (i = 0; i < ffs_num_sectors; i++) {
        if (ffs_sectors[i].fs_length > scratch_len) {
            return FFS_ECORRUPT;
        }
    }

    return 0;
}

static int
ffs_misc_reserve_space_sector(uint32_t *out_offset, uint16_t sector_id,
                              uint16_t size)
{
    const struct ffs_sector *sector;
    uint32_t space;

    sector = ffs_sectors + sector_id;
    space = sector->fs_length - sector->fs_cur;
    if (space >= size) {
        *out_offset = sector->fs_cur;
        return 0;
    }

    return FFS_EFULL;
}

int
ffs_misc_reserve_space(uint16_t *out_sector_id, uint32_t *out_offset,
                       uint16_t size)
{
    uint16_t sector_id;
    int rc;
    int i;

    for (i = 0; i < ffs_num_sectors; i++) {
        if (i != ffs_scratch_sector_id) {
            rc = ffs_misc_reserve_space_sector(out_offset, i, size);
            if (rc == 0) {
                *out_sector_id = i;
                return 0;
            }
        }
    }

    /* No sector can accomodate the request.  Garbage collect until a sector
     * has enough space.
     */
    rc = ffs_gc_until(&sector_id, size);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_misc_reserve_space_sector(out_offset, sector_id, size);
    assert(rc == 0);

    *out_sector_id = sector_id;

    return rc;
}

