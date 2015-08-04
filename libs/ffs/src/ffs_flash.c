#include <assert.h>
#include "hal/hal_flash.h"
#include "ffs/ffs.h"
#include "ffs_priv.h"

int
ffs_flash_read(uint16_t area_idx, uint32_t offset, void *data, uint32_t len)
{
    const struct ffs_area *area;
    int rc;

    assert(area_idx < ffs_num_areas);

    area = ffs_areas + area_idx;

    if (offset + len > area->fa_length) {
        return FFS_ERANGE;
    }

    rc = flash_read(area->fa_offset + offset, data, len);
    return rc;
}

int
ffs_flash_write(uint16_t area_idx, uint32_t offset, const void *data,
                uint32_t len)
{
    struct ffs_area *area;
    int rc;

    assert(area_idx < ffs_num_areas);

    area = ffs_areas + area_idx;
    assert(offset >= area->fa_cur);
    if (offset + len > area->fa_length) {
        return FFS_ERANGE;
    }

    rc = flash_write(area->fa_offset + offset, data, len);
    if (rc != 0) {
        return FFS_EFLASH_ERROR;
    }

    area->fa_cur = offset + len;

    return 0;
}

/** Not thread safe. */
int
ffs_flash_copy(uint16_t area_id_from, uint32_t offset_from,
               uint16_t area_id_to, uint32_t offset_to,
               uint32_t len)
{
    static uint8_t buf[256];
    uint32_t chunk_len;
    int rc;

    while (len > 0) {
        if (len > sizeof buf) {
            chunk_len = sizeof buf;
        } else {
            chunk_len = len;
        }

        rc = ffs_flash_read(area_id_from, offset_from, buf, chunk_len);
        if (rc != 0) {
            return rc;
        }

        rc = ffs_flash_write(area_id_to, offset_to, buf, chunk_len);
        if (rc != 0) {
            return rc;
        }

        offset_from += chunk_len;
        offset_to += chunk_len;
        len -= chunk_len;
    }

    return 0;
}

