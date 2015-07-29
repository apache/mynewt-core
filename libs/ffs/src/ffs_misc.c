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

    if (ffs_scratch_area_id == FFS_AREA_ID_SCRATCH) {
        /* No scratch area. */
        return FFS_ECORRUPT;
    }

    scratch_len = ffs_areas[ffs_scratch_area_id].fs_length;
    for (i = 0; i < ffs_num_areas; i++) {
        if (ffs_areas[i].fs_length > scratch_len) {
            return FFS_ECORRUPT;
        }
    }

    return 0;
}

static int
ffs_misc_reserve_space_area(uint32_t *out_offset, uint16_t area_id,
                              uint16_t size)
{
    const struct ffs_area *area;
    uint32_t space;

    area = ffs_areas + area_id;
    space = area->fs_length - area->fs_cur;
    if (space >= size) {
        *out_offset = area->fs_cur;
        return 0;
    }

    return FFS_EFULL;
}

int
ffs_misc_reserve_space(uint16_t *out_area_id, uint32_t *out_offset,
                       uint16_t size)
{
    uint16_t area_id;
    int rc;
    int i;

    for (i = 0; i < ffs_num_areas; i++) {
        if (i != ffs_scratch_area_id) {
            rc = ffs_misc_reserve_space_area(out_offset, i, size);
            if (rc == 0) {
                *out_area_id = i;
                return 0;
            }
        }
    }

    /* No area can accomodate the request.  Garbage collect until a area
     * has enough space.
     */
    rc = ffs_gc_until(&area_id, size);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_misc_reserve_space_area(out_offset, area_id, size);
    assert(rc == 0);

    *out_area_id = area_id;

    return rc;
}

