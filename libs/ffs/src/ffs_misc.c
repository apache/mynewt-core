#include <assert.h>
#include "os/os_malloc.h"
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

    if (ffs_scratch_area_id == FFS_AREA_ID_NONE) {
        /* No scratch area. */
        return FFS_ECORRUPT;
    }

    scratch_len = ffs_areas[ffs_scratch_area_id].fa_length;
    for (i = 0; i < ffs_num_areas; i++) {
        if (ffs_areas[i].fa_length > scratch_len) {
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
    space = area->fa_length - area->fa_cur;
    if (space >= size) {
        *out_offset = area->fa_cur;
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

void
ffs_misc_invalidate(void)
{
    free(ffs_areas);
    ffs_areas = NULL;
    ffs_num_areas = 0;

    ffs_root_dir = NULL;
    ffs_scratch_area_id = FFS_AREA_ID_NONE;
}

int
ffs_misc_set_num_areas(uint16_t num_areas)
{
    if (num_areas == 0) {
        ffs_misc_invalidate();
        return 0;
    }

    ffs_areas = os_realloc(ffs_areas, num_areas * sizeof *ffs_areas);
    if (ffs_areas == NULL) {
        ffs_misc_invalidate();
        return FFS_ENOMEM;
    }

    ffs_num_areas = num_areas;

    return 0;
}

void
ffs_misc_set_max_block_data_size(void)
{
    uint32_t smallest_area;
    uint32_t half_smallest;
    int i;

    smallest_area = -1;
    for (i = 0; i < ffs_num_areas; i++) {
        if (ffs_areas[i].fa_length < smallest_area) {
            smallest_area = ffs_areas[i].fa_length;
        }
    }

    half_smallest = (smallest_area - sizeof (struct ffs_disk_area)) / 2 -
                    sizeof (struct ffs_disk_block);
    if (half_smallest < FFS_BLOCK_MAX_DATA_SZ_MAX) {
        ffs_block_max_data_sz = half_smallest;
    } else {
        ffs_block_max_data_sz = FFS_BLOCK_MAX_DATA_SZ_MAX;
    }
}

