#include <assert.h>
#include "hal/hal_flash.h"
#include "ffs/ffs.h"
#include "ffs_priv.h"

uint8_t ffs_flash_buf[FFS_FLASH_BUF_SZ];

int
ffs_flash_read(uint8_t area_idx, uint32_t offset, void *data, uint32_t len)
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
ffs_flash_write(uint8_t area_idx, uint32_t offset, const void *data,
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
ffs_flash_copy(uint8_t area_idx_from, uint32_t offset_from,
               uint8_t area_idx_to, uint32_t offset_to,
               uint32_t len)
{
    uint32_t chunk_len;
    int rc;

    while (len > 0) {
        if (len > sizeof ffs_flash_buf) {
            chunk_len = sizeof ffs_flash_buf;
        } else {
            chunk_len = len;
        }

        rc = ffs_flash_read(area_idx_from, offset_from, ffs_flash_buf,
                            chunk_len);
        if (rc != 0) {
            return rc;
        }

        rc = ffs_flash_write(area_idx_to, offset_to, ffs_flash_buf, chunk_len);
        if (rc != 0) {
            return rc;
        }

        offset_from += chunk_len;
        offset_to += chunk_len;
        len -= chunk_len;
    }

    return 0;
}

