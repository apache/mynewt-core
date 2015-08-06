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

    if (ffs_scratch_area_idx == FFS_AREA_ID_NONE) {
        /* No scratch area. */
        return FFS_ECORRUPT;
    }

    scratch_len = ffs_areas[ffs_scratch_area_idx].fa_length;
    for (i = 0; i < ffs_num_areas; i++) {
        if (ffs_areas[i].fa_length > scratch_len) {
            return FFS_ECORRUPT;
        }
    }

    return 0;
}

static int
ffs_misc_reserve_space_area(uint32_t *out_offset, uint8_t area_idx,
                            uint16_t size)
{
    const struct ffs_area *area;
    uint32_t space;

    area = ffs_areas + area_idx;
    space = area->fa_length - area->fa_cur;
    if (space >= size) {
        *out_offset = area->fa_cur;
        return 0;
    }

    return FFS_EFULL;
}

int
ffs_misc_reserve_space(uint8_t *out_area_idx, uint32_t *out_offset,
                       uint16_t size)
{
    uint8_t area_idx;
    int rc;
    int i;

    for (i = 0; i < ffs_num_areas; i++) {
        if (i != ffs_scratch_area_idx) {
            rc = ffs_misc_reserve_space_area(out_offset, i, size);
            if (rc == 0) {
                *out_area_idx = i;
                return 0;
            }
        }
    }

    /* No area can accomodate the request.  Garbage collect until a area
     * has enough space.
     */
    rc = ffs_gc_until(&area_idx, size);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_misc_reserve_space_area(out_offset, area_idx, size);
    assert(rc == 0);

    *out_area_idx = area_idx;

    return rc;
}

int
ffs_misc_set_num_areas(uint8_t num_areas)
{
    if (num_areas == 0) {
        free(ffs_areas);
        ffs_areas = NULL;
    } else {
        ffs_areas = os_realloc(ffs_areas, num_areas * sizeof *ffs_areas);
        if (ffs_areas == NULL) {
            return FFS_ENOMEM;
        }
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

int
ffs_misc_reset(void)
{
    int rc;

    rc = os_mempool_init(&ffs_file_pool, ffs_config.fc_num_files,
                         sizeof (struct ffs_file), ffs_file_mem,
                         "ffs_file_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    rc = os_mempool_init(&ffs_inode_pool, ffs_config.fc_num_inodes,
                         sizeof (struct ffs_inode), ffs_inode_mem,
                         "ffs_inode_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    rc = os_mempool_init(&ffs_block_pool, ffs_config.fc_num_blocks,
                         sizeof (struct ffs_block), ffs_block_mem,
                         "ffs_block_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    ffs_hash_init();

    free(ffs_areas);
    ffs_areas = NULL;
    ffs_num_areas = 0;

    ffs_root_dir = NULL;
    ffs_scratch_area_idx = FFS_AREA_ID_NONE;

    ffs_next_id = 0;

    return 0;
}

