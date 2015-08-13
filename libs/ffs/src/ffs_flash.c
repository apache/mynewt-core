#include <assert.h>
#include "hal/hal_flash.h"
#include "ffs/ffs.h"
#include "ffs_priv.h"

/** A buffer used for flash reads; shared across all of ffs. */
uint8_t ffs_flash_buf[FFS_FLASH_BUF_SZ];

/**
 * Reads a chunk of data from flash.
 *
 * @param area_idx              The index of the area to read from.
 * @param area_offset           The offset within the area to read from.
 * @param data                  On success, the flash contents are written
 *                                  here.
 * @param len                   The number of bytes to read.
 *
 * @return                      0 on success;
 *                              FFS_ERANGE on an attempt to read an invalid
 *                                  address range;
 *                              FFS_EFLASH_ERROR on flash error.
 */
int
ffs_flash_read(uint8_t area_idx, uint32_t area_offset, void *data,
               uint32_t len)
{
    const struct ffs_area *area;
    int rc;

    assert(area_idx < ffs_num_areas);

    area = ffs_areas + area_idx;

    if (area_offset + len > area->fa_length) {
        return FFS_ERANGE;
    }

    rc = flash_read(area->fa_offset + area_offset, data, len);
    if (rc != 0) {
        return FFS_EFLASH_ERROR;
    }

    return 0;
}

/**
 * Writes a chunk of data to flash.
 *
 * @param area_idx              The index of the area to write to.
 * @param area_offset           The offset within the area to write to.
 * @param data                  The data to write to flash.
 * @param len                   The number of bytes to write.
 *
 * @return                      0 on success;
 *                              FFS_ERANGE on an attempt to write to an invalid
 *                                  address range, or on an attempt to perform
 *                                  a non-strictly-sequential write;
 *                              FFS_EFLASH_ERROR on flash error.
 */
int
ffs_flash_write(uint8_t area_idx, uint32_t area_offset, const void *data,
                uint32_t len)
{
    struct ffs_area *area;
    int rc;

    assert(area_idx < ffs_num_areas);
    area = ffs_areas + area_idx;

    if (area_offset + len > area->fa_length) {
        return FFS_ERANGE;
    }

    if (area_offset < area->fa_cur) {
        return FFS_ERANGE;
    }

    rc = flash_write(area->fa_offset + area_offset, data, len);
    if (rc != 0) {
        return FFS_EFLASH_ERROR;
    }

    area->fa_cur = area_offset + len;

    return 0;
}

/**
 * Copies a chunk of data from one region of flash to another.
 *
 * @param area_idx_from         The index of the area to copy from.
 * @param area_offset_from      The offset within the area to copy from.
 * @param area_idx_to           The index of the area to copy to.
 * @param area_offset_to        The offset within the area to copy to.
 * @param len                   The number of bytes to copy.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_flash_copy(uint8_t area_idx_from, uint32_t area_offset_from,
               uint8_t area_idx_to, uint32_t area_offset_to,
               uint32_t len)
{
    uint32_t chunk_len;
    int rc;

    /* Copy data in chunks small enough to fit in the flash buffer. */
    while (len > 0) {
        if (len > sizeof ffs_flash_buf) {
            chunk_len = sizeof ffs_flash_buf;
        } else {
            chunk_len = len;
        }

        rc = ffs_flash_read(area_idx_from, area_offset_from, ffs_flash_buf,
                            chunk_len);
        if (rc != 0) {
            return rc;
        }

        rc = ffs_flash_write(area_idx_to, area_offset_to, ffs_flash_buf,
                             chunk_len);
        if (rc != 0) {
            return rc;
        }

        area_offset_from += chunk_len;
        area_offset_to += chunk_len;
        len -= chunk_len;
    }

    return 0;
}

/**
 * Compresses a flash-area-index,flash-area-offset pair into a 32-bit flash
 * location.
 */
uint32_t
ffs_flash_loc(uint8_t area_idx, uint32_t area_offset)
{
    assert(area_offset <= 0x00ffffff);
    return area_idx << 24 | area_offset;
}

/**
 * Expands a compressed 32-bit flash location into a
 * flash-area-index,flash-area-offset pair.
 */
void
ffs_flash_loc_expand(uint32_t flash_loc, uint8_t *out_area_idx,
                     uint32_t *out_area_offset)
{
    *out_area_idx = flash_loc >> 24;
    *out_area_offset = flash_loc & 0x00ffffff;
}
