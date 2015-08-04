#include <string.h>
#include <inttypes.h>
#include "hal/hal_flash.h"
#include "ffs/ffs.h"
#include "ffsutil/ffsutil.h"
#include "bootutil/crc32.h"
#include "bootutil/image.h"
#include "bootutil_priv.h"

int
boot_crc_is_valid(uint32_t addr, const struct image_header *hdr)
{
    uint32_t crc_len;
    uint32_t off;
    uint32_t crc;
    int chunk_sz;
    int rc;

    static uint8_t buf[256];

    /* Calculate start of crc input, relative to the start of the header. */
    off = IMAGE_HEADER_CRC_OFFSET + sizeof hdr->ih_crc32;

    /* Calculate length of crc input. */
    crc_len = hdr->ih_hdr_size - off + hdr->ih_img_size;

    /* Calculate absolute start of crc input. */
    off += addr;

    /* Apply crc to the image. */
    crc = 0;
    while (crc_len > 0) {
        if (crc_len < sizeof buf) {
            chunk_sz = crc_len;
        } else {
            chunk_sz = sizeof buf;
        }

        rc = flash_read(off, buf, chunk_sz);
        if (rc != 0) {
            return 0;
        }

        crc = crc32(crc, buf, chunk_sz);

        off += chunk_sz;
        crc_len -= chunk_sz;
    }

    return crc == hdr->ih_crc32;
}

static int
boot_vect_read_one(struct image_version *ver, const char *path)
{
    uint32_t len;
    int rc;

    len = sizeof *ver;
    rc = ffsutil_read_file(path, ver, 0, &len);
    if (rc != 0 || len != sizeof *ver) {
        return BOOT_EBADVECT;
    }

    return 0;
}

/**
 * Retrieves from the boot vector the version number of the test image (i.e.,
 * the image that has not been proven stable, and which will only run once).
 *
 * @param out_ver           On success, the test version gets written here.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_read_test(struct image_version *out_ver)
{
    int rc;

    rc = boot_vect_read_one(out_ver, BOOT_PATH_TEST);
    return rc;
}

/**
 * Retrieves from the boot vector the version number of the main image.
 *
 * @param out_ver           On success, the main version gets written here.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_read_main(struct image_version *out_ver)
{
    int rc;

    rc = boot_vect_read_one(out_ver, BOOT_PATH_MAIN);
    return rc;
}

/**
 * Deletes the test image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_delete_test(void)
{
    int rc;

    rc = ffs_unlink(BOOT_PATH_TEST);
    return rc;
}

/**
 * Deletes the main image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_delete_main(void)
{
    int rc;

    rc = ffs_unlink(BOOT_PATH_MAIN);
    return rc;
}

static int
boot_read_image_header(struct image_header *out_hdr, uint32_t flash_address)
{
    int rc;

    rc = flash_read(flash_address, out_hdr, sizeof *out_hdr);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    if (out_hdr->ih_magic != IMAGE_MAGIC) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}

/**
 * Reads the header of each image present in flash.  Headers corresponding to
 * empty image slots are filled with 0xff bytes.
 *
 * @param out_headers           Points to an array of image headers.  Each
 *                                  element is filled with the header of the
 *                                  corresponding image in flash.
 * @param addresses             An array containing the flash addresses of each
 *                                  image slot.
 * @param num_addresses         The number of headers to read.  This should
 *                                  also be equal to the lengths of the
 *                                  out_headers and addresses arrays.
 */
void
boot_read_image_headers(struct image_header *out_headers,
                        const uint32_t *addresses, int num_addresses)
{
    struct image_header *hdr;
    int rc;
    int i;

    for (i = 0; i < num_addresses; i++) {
        hdr = out_headers + i;
        rc = boot_read_image_header(hdr, addresses[i]);
        if (rc != 0 || hdr->ih_magic != IMAGE_MAGIC) {
            memset(hdr, 0xff, sizeof *hdr);
        }
    }
}

/**
 * Reads the boot status from the flash file system.  The boot status contains
 * the current state of an interrupted image copy operation.  If the boot
 * status is not present in the file system, the implication is that there is
 * no copy operation in progress.
 *
 * @param out_status            On success, the boot status gets written here.
 * @param out_entries           On success, the array of boot entries gets
 *                                  written here.
 * @param num_areas             The number of flash areas capable of storing
 *                                  image data.  This is equal to the length of
 *                                  the out_entries array.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_read_status(struct boot_status *out_status,
                 struct boot_status_entry *out_entries,
                 int num_areas)
{
    struct ffs_file *file;
    uint32_t len;
    int rc;
    int i;

    rc = ffs_open(&file, BOOT_PATH_STATUS, FFS_ACCESS_READ);
    if (rc != 0) {
        rc = BOOT_EBADSTATUS;
        goto done;
    }

    len = sizeof *out_status;
    rc = ffs_read(file, out_status, &len);
    if (rc != 0 || len != sizeof *out_status) {
        rc = BOOT_EBADSTATUS;
        goto done;
    }

    len = num_areas * sizeof *out_entries;
    rc = ffs_read(file, out_entries, &len);
    if (rc != 0 || len != num_areas * sizeof *out_entries) {
        rc = BOOT_EBADSTATUS;
        goto done;
    }

    if (out_status->bs_img1_length == 0xffffffff) {
        out_status->bs_img1_length = 0;
    }
    if (out_status->bs_img2_length == 0xffffffff) {
        out_status->bs_img2_length = 0;
    }

    for (i = 0; i < num_areas; i++) {
        if (out_entries[i].bse_image_num == 0 &&
            out_status->bs_img1_length == 0) {

            rc = BOOT_EBADSTATUS;
            goto done;
        }
        if (out_entries[i].bse_image_num == 1 &&
            out_status->bs_img2_length == 0) {

            rc = BOOT_EBADSTATUS;
            goto done;
        }
    }

    rc = 0;

done:
    ffs_close(file);
    return rc;
}

/**
 * Writes the supplied boot status to the flash file system.  The boot status
 * contains the current state of an in-progress image copy operation.
 *
 * @param status                The boot status base to write.
 * @param entries               The array of boot status entries to write.
 * @param num_areas             The number of flash areas capable of storing
 *                                  image data.  This is equal to the length of
 *                                  the entries array.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_write_status(const struct boot_status *status,
                  const struct boot_status_entry *entries,
                  int num_areas)
{
    struct ffs_file *file;
    int rc;

    rc = ffs_open(&file, BOOT_PATH_STATUS,
                  FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE);
    if (rc != 0) {
        rc = BOOT_EFILE;
        goto done;
    }

    rc = ffs_write(file, status, sizeof *status);
    if (rc != 0) {
        rc = BOOT_EFILE;
        goto done;
    }

    rc = ffs_write(file, entries, num_areas * sizeof *entries);
    if (rc != 0) {
        rc = BOOT_EFILE;
        goto done;
    }

    rc = 0;

done:
    ffs_close(file);
    return rc;
}

/**
 * Erases the boot status from the flash file system.  The boot status
 * contains the current state of an in-progress image copy operation.  By
 * erasing the boot status, it is implied that there is no copy operation in
 * progress.
 */
void
boot_clear_status(void)
{
    ffs_unlink(BOOT_PATH_STATUS);
}
