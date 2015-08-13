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

/**
 * Reserves the specified number of bytes within the specified area.
 *
 * @param area_idx              The index of the area to reserve from.
 * @param space                 The number of bytes of free space required.
 * @param out_area_offset       On success, the offset within the area gets
 *                                  written here.
 *
 * @return                      0 on success;
 *                              FFS_EFULL if the area has insufficient free
 *                                  space.
 */
static int
ffs_misc_reserve_space_area(uint8_t area_idx, uint16_t space,
                            uint32_t *out_area_offset)
{
    const struct ffs_area *area;
    uint32_t available;

    area = ffs_areas + area_idx;
    available = area->fa_length - area->fa_cur;
    if (available >= space) {
        *out_area_offset = area->fa_cur;
        return 0;
    }

    return FFS_EFULL;
}

/**
 * Finds an area that can accommodate an object of the specified size.  If no
 * such area exists, this function performs a garbage collection cycle.
 *
 * @param space                 The number of bytes of free space required.
 * @param out_area_idx          On success, the index of the suitable area gets
 *                                  written here.
 * @param out_area_offset       On success, the offset within the suitable area
 *                                  gets written here.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_misc_reserve_space(uint16_t space,
                       uint8_t *out_area_idx, uint32_t *out_area_offset)
{
    uint8_t area_idx;
    int rc;
    int i;

    for (i = 0; i < ffs_num_areas; i++) {
        if (i != ffs_scratch_area_idx) {
            rc = ffs_misc_reserve_space_area(i, space, out_area_offset);
            if (rc == 0) {
                *out_area_idx = i;
                return 0;
            }
        }
    }

    /* No area can accomodate the request.  Garbage collect until an area
     * has enough space.
     */
    rc = ffs_gc_until(space, &area_idx);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_misc_reserve_space_area(area_idx, space, out_area_offset);
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
        ffs_areas = realloc(ffs_areas, num_areas * sizeof *ffs_areas);
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

    /* XXX: Ensure no blocks exist with too large of a data size. */
}

/**
 * Fully resets the ffs RAM representation.
 */
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

    rc = os_mempool_init(&ffs_inode_entry_pool, ffs_config.fc_num_inodes,
                         sizeof (struct ffs_inode_entry), ffs_inode_mem,
                         "ffs_inode_entry_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    rc = os_mempool_init(&ffs_hash_entry_pool, ffs_config.fc_num_blocks,
                         sizeof (struct ffs_hash_entry), ffs_hash_entry_mem,
                         "ffs_hash_entry_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    ffs_hash_init();

    free(ffs_areas);
    ffs_areas = NULL;
    ffs_num_areas = 0;

    ffs_root_dir = NULL;
    ffs_scratch_area_idx = FFS_AREA_ID_NONE;

    ffs_hash_next_file_id = FFS_ID_FILE_MIN;
    ffs_hash_next_dir_id = FFS_ID_DIR_MIN;
    ffs_hash_next_block_id = FFS_ID_BLOCK_MIN;

    return 0;
}

