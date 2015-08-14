#include <assert.h>
#include <string.h>
#include "hal/hal_flash.h"
#include "ffs_priv.h"
#include "ffs/ffs.h"

/**
 * Turns a scratch area into a non-scratch area.  If the specified area is not
 * actually a scratch area, this function falls back to a slower full format
 * operation.
 */
int
ffs_format_from_scratch_area(uint8_t area_idx, uint8_t area_id)
{
    struct ffs_disk_area disk_area;
    int rc;

    assert(area_idx < ffs_num_areas);
    rc = ffs_flash_read(area_idx, 0, &disk_area, sizeof disk_area);
    if (rc != 0) {
        return rc;
    }

    ffs_areas[area_idx].fa_id = area_id;
    if (!ffs_area_is_scratch(&disk_area)) {
        rc = ffs_format_area(area_idx, 0);
        if (rc != 0) {
            return rc;
        }
    } else {
        disk_area.fda_id = area_id;
        rc = ffs_flash_write(area_idx, FFS_AREA_OFFSET_ID,
                             &disk_area.fda_id, sizeof disk_area.fda_id);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

/**
 * Formats a single scratch area.
 */
int
ffs_format_area(uint8_t area_idx, int is_scratch)
{
    struct ffs_disk_area disk_area;
    struct ffs_area *area;
    uint32_t write_len;
    int rc;

    area = ffs_areas + area_idx;

    rc = flash_erase(area->fa_offset, area->fa_length);
    if (rc != 0) {
        return rc;
    }
    area->fa_cur = 0;

    ffs_area_to_disk(area, &disk_area);

    if (is_scratch) {
        ffs_areas[area_idx].fa_id = FFS_AREA_ID_NONE;
        write_len = sizeof disk_area - sizeof disk_area.fda_id;
    } else {
        write_len = sizeof disk_area;
    }

    rc = ffs_flash_write(area_idx, 0, &disk_area.fda_magic, write_len);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Erases all the specified areas and initializes them with a clean ffs
 * file system.
 *
 * @param area_descs        The set of areas to format.
 *
 * @return                  0 on success;
 *                          nonzero on failure.
 */
int
ffs_format_full(const struct ffs_area_desc *area_descs)
{
    int rc;
    int i;

    /* Start from a clean state. */
    ffs_misc_reset();

    /* Select largest area to be the initial scratch area. */
    ffs_scratch_area_idx = 0;
    for (i = 1; area_descs[i].fad_length != 0; i++) {
        if (i >= FFS_MAX_AREAS) {
            rc = FFS_EINVAL;
            goto err;
        }

        if (area_descs[i].fad_length >
            area_descs[ffs_scratch_area_idx].fad_length) {

            ffs_scratch_area_idx = i;
        }
    }

    rc = ffs_misc_set_num_areas(i);
    if (rc != 0) {
        goto err;
    }

    for (i = 0; i < ffs_num_areas; i++) {
        ffs_areas[i].fa_offset = area_descs[i].fad_offset;
        ffs_areas[i].fa_length = area_descs[i].fad_length;
        ffs_areas[i].fa_cur = 0;
        ffs_areas[i].fa_gc_seq = 0;

        if (i == ffs_scratch_area_idx) {
            ffs_areas[i].fa_id = FFS_AREA_ID_NONE;
        } else {
            ffs_areas[i].fa_id = i;
        }

        rc = ffs_format_area(i, i == ffs_scratch_area_idx);
        if (rc != 0) {
            goto err;
        }
    }

    rc = ffs_misc_validate_scratch();
    if (rc != 0) {
        goto err;
    }

    /* Create root directory. */
    rc = ffs_file_new(NULL, "", 0, 1, &ffs_root_dir);
    if (rc != 0) {
        goto err;
    }

    rc = ffs_misc_validate_root_dir();
    if (rc != 0) {
        goto err;
    }

    rc = ffs_misc_set_max_block_data_len(0);
    if (rc != 0) {
        goto err;
    }

    return 0;

err:
    ffs_misc_reset();
    return rc;
}
