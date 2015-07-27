#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include "hal/hal_flash.h"
#include "ffs/ffs.h"
#include "bootutil/bootutil.h"

/* XXX: Where? */
#include "stm32f4xx/stm32f4xx.h"

/** Image slots. */
static const uint32_t boot_img_addrs[2] = {
    0x08020000,
    0x08080000,
};

static const struct ffs_sector_desc loader_sector_descs[] = {
    [0] =  { 0x08000000, 16 * 1024 },
    [1] =  { 0x08004000, 16 * 1024 },
    [2] =  { 0x08008000, 16 * 1024 },
    [3] =  { 0x0800c000, 16 * 1024 },
    [4] =  { 0x08010000, 64 * 1024 },
    [5] =  { 0x08020000, 128 * 1024 },
    [6] =  { 0x08040000, 128 * 1024 },
    [7] =  { 0x08060000, 128 * 1024 },
    [8] =  { 0x08080000, 128 * 1024 },
    [9] =  { 0x080a0000, 128 * 1024 },
    [10] = { 0x080c0000, 128 * 1024 },
    [11] = { 0x080e0000, 128 * 1024 },
    { 0, 0 },
};

static const uint16_t boot_img_parts[] = {
    5, 6, 7, 8, 9, 10, 11
};

#define BOOT_NUM_IMG_PARTS \
    ((int)(sizeof boot_img_parts / sizeof boot_img_parts[0]))

static const uint16_t boot_sector_idx_scratch = 11;

static struct image_header boot_img_hdrs[2];
static int boot_num_hdrs;
static struct boot_status boot_status;
static struct boot_status_entry boot_status_entries[BOOT_NUM_IMG_PARTS];

static void
boot_jump(const struct image_header *hdr)
{
    typedef void jump_fn(void);

    uint32_t base0entry;
    uint32_t img_start;
    uint32_t jump_addr;
    jump_fn *fn;

    img_start = boot_img_addrs[0] + hdr->hdr_size;

    /* First word contains initial MSP value. */
    __set_MSP(*(uint32_t *)img_start);

    /* Second word contains address of entry point (Reset_Handler). */
    base0entry = *(uint32_t *)(img_start + 4);
    jump_addr = base0entry;
    fn = (jump_fn *)jump_addr;

    /* Remap memory such that flash gets mapped to the code region. */
    SYSCFG->MEMRMP = 0;
    __DSB();

    /* Jump to image. */
    fn();
}

static int
boot_select_image_slot(void)
{
    struct image_version cur_ver;
    int rc;
    int i;

    rc = boot_vect_read_cur(&cur_ver);
    if (rc == 0) {
        for (i = 0; i < 2; i++) {
            if (memcmp(&boot_img_hdrs[i].ih_ver, &cur_ver,
                       sizeof cur_ver) == 0) {
                return i;
            }
        }
    }

    return -1;
}

/** Returns sector num. */
static int
boot_find_image_part(int image_num, int part_num)
{
    int i;

    for (i = 0; i < BOOT_NUM_IMG_PARTS; i++) {
        if (boot_status_entries[i].bse_image_num == image_num &&
            boot_status_entries[i].bse_part_num == part_num) {

            return boot_img_parts[i];
        }
    }

    return -1;
}

static int
boot_erase_sector(int sector_idx)
{
    const struct ffs_sector_desc *sector_desc;
    int rc;

    assert(sector_idx >= 0 && sector_idx < BOOT_NUM_IMG_PARTS);

    sector_desc = loader_sector_descs + sector_idx;
    rc = flash_erase_sector(sector_desc->fsd_offset);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

static int
boot_copy_sector(int from_sector_idx, int to_sector_idx)
{
    const struct ffs_sector_desc *from_sector_desc;
    const struct ffs_sector_desc *to_sector_desc;
    uint32_t from_addr;
    uint32_t to_addr;
    uint32_t off;
    int chunk_sz;
    int rc;

    static uint8_t buf[1024];

    from_sector_desc = loader_sector_descs + from_sector_idx;
    to_sector_desc = loader_sector_descs + to_sector_idx;

    assert(to_sector_desc->fsd_length >= from_sector_desc->fsd_length);

    off = 0;
    while (off < from_sector_desc->fsd_length) {
        if (from_sector_desc->fsd_length - off > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = from_sector_desc->fsd_length - off;
        }

        from_addr = from_sector_desc->fsd_offset + off;
        rc = flash_read(buf, from_addr, chunk_sz);
        if (rc != 0) {
            return rc;
        }

        to_addr = to_sector_desc->fsd_offset + off;
        rc = flash_write(buf, to_addr, chunk_sz);
        if (rc != 0) {
            return rc;
        }

        off += chunk_sz;
    }

    return 0;
}

static int
boot_copy_image(uint32_t img1_length, uint32_t img2_length)
{
    const struct ffs_sector_desc *sector_desc;
    uint32_t off;
    int src_sector_idx;
    int dst_sector_idx;
    int part_num;
    int rc;

    part_num = 0;
    off = 0;
    while (off < img2_length) {
        src_sector_idx = boot_find_image_part(1, part_num);
        if (src_sector_idx == -1) {
            return BOOT_EBADIMAGE;
        }

        dst_sector_idx = boot_img_parts[part_num];
        if (src_sector_idx != dst_sector_idx) {
            if (src_sector_idx != boot_sector_idx_scratch) {
                rc = boot_erase_sector(boot_sector_idx_scratch);
                if (rc != 0) {
                    return rc;
                }
            }

            rc = boot_copy_sector(dst_sector_idx, boot_sector_idx_scratch);
            if (rc != 0) {
                return rc;
            }

            /* XXX: Record status to ffs. */

            rc = boot_erase_sector(dst_sector_idx);
            if (rc != 0) {
                return rc;
            }

            rc = boot_copy_sector(boot_sector_idx_scratch, dst_sector_idx);
            if (rc != 0) {
                return rc;
            }
        }

        part_num++;

        sector_desc = loader_sector_descs + dst_sector_idx;
        off += sector_desc->fsd_length;
    }

    dst_sector_idx = src_sector_idx + 1;
    while (off < img1_length) {
        rc = boot_erase_sector(dst_sector_idx);
        if (rc != 0) {
            return rc;
        }

        src_sector_idx = boot_img_parts[part_num];
        rc = boot_copy_sector(src_sector_idx, dst_sector_idx);
        if (rc != 0) {
            return rc;
        }

        sector_desc = loader_sector_descs + dst_sector_idx;
        off += sector_desc->fsd_length;

        dst_sector_idx++;
        part_num++;
    }

    return 0;
}

int
main(void)
{
    const struct image_header *boot_hdr;
    int slot;
    int rc;

    rc = flash_init();
    assert(rc == 0);

    rc = ffs_init();
    assert(rc == 0);

    /* Look for an ffs file system in internal flash. */
    ffs_detect(loader_sector_descs);

    rc = boot_read_status(&boot_status, boot_status_entries,
                          BOOT_NUM_IMG_PARTS);
    if (rc == 0) {
        /* We are resuming a partial image copy. */
        rc = boot_copy_image(boot_status.bs_img1_length,
                             boot_status.bs_img2_length);
        assert(rc == 0); // XXX;
    }

    boot_read_image_headers(boot_img_hdrs, &boot_num_hdrs, boot_img_addrs, 2);

    slot = boot_select_image_slot();
    if (slot == -1) {
        /* Current image is not in flash.  Fall back to last-known-good. */
        boot_vect_repair();
        slot = boot_select_image_slot();
        if (slot == -1) {
            /* Last-known-good image isn't present either.  Just boot from the
             * first image slot.
             */
            slot = 0;
        }
    }

    switch (slot) {
    case 0:
        boot_hdr = &boot_img_hdrs[0];
        break;

    case 1:
        rc = boot_copy_image(boot_img_hdrs[0].img_size,
                             boot_img_hdrs[1].img_size);
        assert(rc == 0); // XXX;
        boot_vect_rotate();

        boot_hdr = &boot_img_hdrs[1];
        break;

    default:
        assert(0);
        break;
    }

    boot_jump(boot_hdr);

    return 0;
}

