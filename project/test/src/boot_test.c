#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "hal/hal_flash.h"
#include "ffs/ffs.h"
#include "ffsutil/ffsutil.h"
#include "bootutil/image.h"
#include "bootutil/loader.h"
#include "../src/bootutil_priv.h"

#define BOOT_TEST_HEADER_SIZE       0x200

/** Image slots. */
static uint32_t boot_test_img_addrs[2] = {
    0x00020000,
    0x00080000,
};

/** Internal flash layout. */
static struct ffs_sector_desc boot_test_sector_descs[] = {
    [0] =  { 0x00000000, 16 * 1024 },
    [1] =  { 0x00004000, 16 * 1024 },
    [2] =  { 0x00008000, 16 * 1024 },
    [3] =  { 0x0000c000, 16 * 1024 },
    [4] =  { 0x00010000, 64 * 1024 },
    [5] =  { 0x00020000, 128 * 1024 },
    [6] =  { 0x00040000, 128 * 1024 },
    [7] =  { 0x00060000, 128 * 1024 },
    [8] =  { 0x00080000, 128 * 1024 },
    [9] =  { 0x000a0000, 128 * 1024 },
    [10] = { 0x000c0000, 128 * 1024 },
    [11] = { 0x000e0000, 128 * 1024 },
    { 0, 0 },
};

static const struct ffs_sector_desc boot_test_format_descs[] = {
    [0] =  { 0x00004000, 16 * 1024 },
    [1] =  { 0x00008000, 16 * 1024 },
    [2] =  { 0x0000c000, 16 * 1024 },
    { 0, 0 },
};

/** Contains indices of the sectors which can contain image data. */
static uint16_t boot_test_img_sectors[] = {
    5, 6, 7, 8, 9, 10, 11
};

#define BOOT_TEST_NUM_IMG_SECTORS \
    ((int)(sizeof boot_test_img_sectors / sizeof boot_test_img_sectors[0]))

#define BOOT_TEST_SECTOR_IDX_SCRATCH 11
#define BOOT_TEST_IMG_SECTOR_IDX_SCRATCH 6

static uint8_t
boot_test_util_byte_at(int img_msb, uint32_t image_offset)
{
    uint32_t u32;
    uint8_t *u8p;

    assert(image_offset < 0x01000000);
    u32 = image_offset + (img_msb << 24);
    u8p = (void *)&u32;
    return u8p[image_offset % 4];
}

static void
boot_test_util_init_flash(void)
{
    const struct ffs_sector_desc *sector_desc;
    int rc;

    rc = flash_init();
    assert(rc == 0);

    for (sector_desc = boot_test_sector_descs;
         sector_desc->fsd_length != 0;
         sector_desc++) {

        rc = flash_erase(sector_desc->fsd_offset, sector_desc->fsd_length);
        assert(rc == 0);
    }

    rc = ffs_init();
    assert(rc == 0);

    rc = ffs_format(boot_test_format_descs);
    assert(rc == 0);

    rc = ffs_mkdir("/boot");
    assert(rc == 0);
}

static void
boot_test_util_copy_sector(int from_sector_idx, int to_sector_idx)
{
    const struct ffs_sector_desc *from_sector_desc;
    const struct ffs_sector_desc *to_sector_desc;
    void *buf;
    int rc;

    from_sector_desc = boot_test_sector_descs + from_sector_idx;
    to_sector_desc = boot_test_sector_descs + to_sector_idx;

    assert(from_sector_desc->fsd_length == to_sector_desc->fsd_length);

    buf = malloc(from_sector_desc->fsd_length);
    assert(buf != NULL);

    rc = flash_read(buf, from_sector_desc->fsd_offset,
                    from_sector_desc->fsd_length);
    assert(rc == 0);

    rc = flash_erase(to_sector_desc->fsd_offset, to_sector_desc->fsd_length);
    assert(rc == 0);

    rc = flash_write(buf, to_sector_desc->fsd_offset,
                     to_sector_desc->fsd_length);
    assert(rc == 0);

    free(buf);
}

static void
boot_test_util_swap_sectors(int sector_idx1, int sector_idx2)
{
    const struct ffs_sector_desc *sector_desc1;
    const struct ffs_sector_desc *sector_desc2;
    void *buf1;
    void *buf2;
    int rc;

    sector_desc1 = boot_test_sector_descs + sector_idx1;
    sector_desc2 = boot_test_sector_descs + sector_idx2;

    assert(sector_desc1->fsd_length == sector_desc2->fsd_length);

    buf1 = malloc(sector_desc1->fsd_length);
    assert(buf1 != NULL);

    buf2 = malloc(sector_desc2->fsd_length);
    assert(buf2 != NULL);

    rc = flash_read(buf1, sector_desc1->fsd_offset, sector_desc1->fsd_length);
    assert(rc == 0);

    rc = flash_read(buf2, sector_desc2->fsd_offset, sector_desc2->fsd_length);
    assert(rc == 0);

    rc = flash_erase(sector_desc1->fsd_offset, sector_desc1->fsd_length);
    assert(rc == 0);

    rc = flash_erase(sector_desc2->fsd_offset, sector_desc2->fsd_length);
    assert(rc == 0);

    rc = flash_write(buf2, sector_desc1->fsd_offset, sector_desc1->fsd_length);
    assert(rc == 0);

    rc = flash_write(buf1, sector_desc2->fsd_offset, sector_desc2->fsd_length);
    assert(rc == 0);

    free(buf1);
    free(buf2);
}

static void
boot_test_util_write_image(const struct image_header *hdr, int slot)
{
    uint32_t image_off;
    uint32_t off;
    uint8_t buf[256];
    int chunk_sz;
    int rc;
    int i;

    assert(slot == 0 || slot == 1);

    off = boot_test_img_addrs[slot];

    rc = flash_write(hdr, off, sizeof *hdr);
    assert(rc == 0);

    off += hdr->ih_hdr_size;

    image_off = 0;
    while (image_off < hdr->ih_img_size) {
        if (hdr->ih_img_size - image_off > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = hdr->ih_img_size - image_off;
        }

        for (i = 0; i < chunk_sz; i++) {
            buf[i] = boot_test_util_byte_at(slot, image_off + i);
        }

        rc = flash_write(buf, off + image_off, chunk_sz);
        assert(rc == 0);

        image_off += chunk_sz;
    }
}

static void
boot_test_util_verify_sector(const struct ffs_sector_desc *sector_desc,
                             const struct image_header *hdr,
                             uint32_t image_addr, int img_msb)
{
    struct image_header temp_hdr;
    uint32_t sector_end;
    uint32_t img_size;
    uint32_t img_off;
    uint32_t img_end;
    uint32_t addr;
    uint8_t buf[256];
    int rem_sector;
    int past_image;
    int chunk_sz;
    int rem_img;
    int rc;
    int i;

    addr = sector_desc->fsd_offset;

    if (hdr != NULL) {
        img_size = hdr->ih_img_size;

        if (addr == image_addr) {
            rc = flash_read(&temp_hdr, image_addr, sizeof temp_hdr);
            assert(rc == 0);
            assert(memcmp(&temp_hdr, hdr, sizeof *hdr) == 0);

            addr += hdr->ih_hdr_size;
        }
    } else {
        img_size = 0;
    }

    sector_end = sector_desc->fsd_offset + sector_desc->fsd_length;
    img_end = image_addr + img_size;
    past_image = addr >= img_end;

    while (addr < sector_end) {
        rem_sector = sector_end - addr;
        rem_img = img_end - addr;

        if (hdr != NULL) {
            img_off = addr - image_addr - hdr->ih_hdr_size;
        }

        if (rem_sector > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = rem_sector;
        }

        rc = flash_read(buf, addr, chunk_sz);
        assert(rc == 0);

        for (i = 0; i < chunk_sz; i++) {
            if (rem_img > 0) {
                assert(buf[i] == boot_test_util_byte_at(img_msb,
                                                        img_off + i));
            } else if (past_image) {
                assert(buf[i] == 0xff);
            }
        }

        addr += chunk_sz;
    }
}

static void
boot_test_util_verify_status_clear(void)
{
    struct ffs_file *file;
    int rc;

    rc = ffs_open(&file, BOOT_PATH_STATUS, FFS_ACCESS_READ);
    assert(rc == FFS_ENOENT);
}

static void
boot_test_util_verify_flash(const struct image_header *hdr0, int orig_slot_0,
                            const struct image_header *hdr1, int orig_slot_1)
{
    const struct ffs_sector_desc *sector_desc;
    int sector_idx;

    sector_idx = boot_test_img_sectors[0];

    while (1) {
        sector_desc = boot_test_sector_descs + sector_idx;
        if (sector_desc->fsd_offset == boot_test_img_addrs[1]) {
            break;
        }

        boot_test_util_verify_sector(sector_desc, hdr0,
                                     boot_test_img_addrs[0], orig_slot_0);
        sector_idx++;
    }

    while (1) {
        if (sector_idx == BOOT_TEST_SECTOR_IDX_SCRATCH) {
            break;
        }

        sector_desc = boot_test_sector_descs + sector_idx;
        boot_test_util_verify_sector(sector_desc, hdr1,
                                     boot_test_img_addrs[1], orig_slot_1);
        sector_idx++;
    }
}

static void
boot_test_nv_ns_10(void)
{
    struct boot_rsp rsp;
    int rc;


    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tno-vector no-status 1-0 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr, 0, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

static void
boot_test_nv_ns_01(void)
{
    struct boot_rsp rsp;
    int rc;


    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 10 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tno-vector no-status 0-1 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 1);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr, 1, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

static void
boot_test_nv_ns_11(void)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tno-vector no-status 1-1 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr0, sizeof hdr0) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr0, 0, &hdr1, 1);
    boot_test_util_verify_status_clear();
}

static void
boot_test_vm_ns_10(void)
{
    struct boot_rsp rsp;
    int rc;


    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tvector-main no-status 1-0 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);

    rc = ffsutil_write_file(BOOT_PATH_MAIN, &hdr.ih_ver, sizeof hdr.ih_ver);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr, 0, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

static void
boot_test_vm_ns_01(void)
{
    struct boot_rsp rsp;
    int rc;


    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 10 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tvector-main no-status 0-1 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 1);

    rc = ffsutil_write_file(BOOT_PATH_MAIN, &hdr.ih_ver, sizeof hdr.ih_ver);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr, 1, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

static void
boot_test_vm_ns_11_a(void)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tvector-main no-status 1-1-a test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);

    rc = ffsutil_write_file(BOOT_PATH_MAIN, &hdr0.ih_ver, sizeof hdr0.ih_ver);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr0, sizeof hdr0) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr0, 0, &hdr1, 1);
    boot_test_util_verify_status_clear();
}

static void
boot_test_vm_ns_11_b(void)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tvector-main no-status 1-1-b test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);

    rc = ffsutil_write_file(BOOT_PATH_MAIN, &hdr1.ih_ver, sizeof hdr1.ih_ver);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();
}

static void
boot_test_vm_ns_11_2sectors(void)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 196 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tvector-main no-status 1-1-2sectors test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);

    rc = ffsutil_write_file(BOOT_PATH_MAIN, &hdr1.ih_ver, sizeof hdr1.ih_ver);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();
}

static void
boot_test_nv_bs_10(void)
{
    struct boot_status_entry entries[BOOT_TEST_NUM_IMG_SECTORS];
    struct boot_status status;
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tno-vector basic-status 1-0 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    boot_test_util_swap_sectors(boot_test_img_sectors[0],
                                BOOT_TEST_SECTOR_IDX_SCRATCH);

    memset(&status, 0xff, sizeof status);
    status.bs_img2_length = hdr.ih_img_size;

    memset(entries, 0xff, sizeof entries);
    entries[BOOT_TEST_IMG_SECTOR_IDX_SCRATCH].bse_image_num = 1;
    entries[BOOT_TEST_IMG_SECTOR_IDX_SCRATCH].bse_part_num = 0;

    rc = boot_write_status(&status, entries, BOOT_TEST_NUM_IMG_SECTORS);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr, 0, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

static void
boot_test_nv_bs_11(void)
{
    struct boot_status_entry entries[BOOT_TEST_NUM_IMG_SECTORS];
    struct boot_status status;
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 17 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 1, 5, 5 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tno-vector basic-status 1-1 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_copy_sector(boot_test_img_sectors[0],
                               BOOT_TEST_SECTOR_IDX_SCRATCH);

    memset(&status, 0xff, sizeof status);
    status.bs_img1_length = hdr0.ih_img_size;
    status.bs_img2_length = hdr1.ih_img_size;

    memset(entries, 0xff, sizeof entries);
    entries[3].bse_image_num = 1;
    entries[3].bse_part_num = 0;
    entries[BOOT_TEST_IMG_SECTOR_IDX_SCRATCH].bse_image_num = 0;
    entries[BOOT_TEST_IMG_SECTOR_IDX_SCRATCH].bse_part_num = 0;

    rc = boot_write_status(&status, entries, BOOT_TEST_NUM_IMG_SECTORS);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();
}

static void
boot_test_nv_bs_11_2sectors(void)
{
    struct boot_status_entry entries[BOOT_TEST_NUM_IMG_SECTORS];
    struct boot_status status;
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 150 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 190 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tno-vector basic-status 1-1-2sectors test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_swap_sectors(boot_test_img_sectors[0],
                                boot_test_img_sectors[3]);

    memset(&status, 0xff, sizeof status);
    status.bs_img1_length = hdr0.ih_img_size;
    status.bs_img2_length = hdr1.ih_img_size;

    memset(entries, 0xff, sizeof entries);
    entries[0].bse_image_num = 1;
    entries[0].bse_part_num = 0;
    entries[1].bse_image_num = 0;
    entries[1].bse_part_num = 1;
    entries[3].bse_image_num = 0;
    entries[3].bse_part_num = 0;
    entries[4].bse_image_num = 1;
    entries[4].bse_part_num = 1;

    rc = boot_write_status(&status, entries, BOOT_TEST_NUM_IMG_SECTORS);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();
}

static void
boot_test_vb_ns_11(void)
{
    struct boot_rsp rsp;
    int rc;
    int i;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = 0,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_crc32 = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_sector_descs = boot_test_sector_descs,
        .br_image_addrs = boot_test_img_addrs,
        .br_image_sectors = boot_test_img_sectors,
        .br_scratch_sector_idx = BOOT_TEST_SECTOR_IDX_SCRATCH,
        .br_num_image_sectors = BOOT_TEST_NUM_IMG_SECTORS,
        .br_num_slots = 2,
    };

    printf("\tvector-both no-status 1-1 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);

    rc = ffsutil_write_file(BOOT_PATH_MAIN, &hdr0.ih_ver, sizeof hdr0.ih_ver);
    assert(rc == 0);

    rc = ffsutil_write_file(BOOT_PATH_TEST, &hdr1.ih_ver, sizeof hdr1.ih_ver);
    assert(rc == 0);

    /* First boot should use the test image. */
    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();

    /* Ensure all subsequent boots use the main image. */
    for (i = 0; i < 10; i++) {
        rc = boot_go(&req, &rsp);
        assert(rc == 0);

        assert(memcmp(rsp.br_hdr, &hdr0, sizeof hdr0) == 0);
        assert(rsp.br_image_addr == boot_test_img_addrs[0]);

        boot_test_util_verify_flash(&hdr0, 0, &hdr1, 1);
        boot_test_util_verify_status_clear();
    }
}

void
boot_test(void)
{
    printf("boot loader testing\n");

    boot_test_nv_ns_10();
    boot_test_nv_ns_01();
    boot_test_nv_ns_11();
    boot_test_vm_ns_10();
    boot_test_vm_ns_01();
    boot_test_vm_ns_11_a();
    boot_test_vm_ns_11_b();
    boot_test_vm_ns_11_2sectors();
    boot_test_nv_bs_10();
    boot_test_nv_bs_11();
    boot_test_nv_bs_11_2sectors();
    boot_test_vb_ns_11();

    printf("\n");
}
