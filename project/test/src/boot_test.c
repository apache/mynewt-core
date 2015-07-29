#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "hal/hal_flash.h"
#include "ffs/ffs.h"
#include "ffsutil/ffsutil.h"
#include "bootutil/image.h"
#include "bootutil/loader.h"

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
boot_test_util_write_image(const struct image_header *hdr, int slot)
{
    uint32_t image_off;
    uint32_t off;
    uint8_t buf[256];
    int chunk_sz;
    int rc;

    assert(slot == 0 || slot == 1);

    off = boot_test_img_addrs[slot];

    rc = flash_write(hdr, off, sizeof *hdr);
    assert(rc == 0);

    off += hdr->ih_hdr_size;

    memset(buf, hdr->ih_ver.iv_major, sizeof buf);
    image_off = 0;
    while (image_off < hdr->ih_img_size) {
        if (hdr->ih_img_size - image_off > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = hdr->ih_img_size - image_off;
        }
        rc = flash_write(buf, off + image_off, chunk_sz);
        assert(rc == 0);

        image_off += chunk_sz;
    }
}

static void
boot_test_util_verify_sector(const struct ffs_sector_desc *sector_desc,
                             const struct image_header *hdr,
                             uint32_t image_addr, int slot_num)
{
    struct image_header temp_hdr;
    uint32_t sector_end;
    uint32_t img_size;
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

        if (rem_sector > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = rem_sector;
        }

        rc = flash_read(buf, addr, chunk_sz);
        assert(rc == 0);

        for (i = 0; i < chunk_sz; i++) {
            if (rem_img > 0) {
                assert(buf[i] == hdr->ih_ver.iv_major);
            } else if (past_image) {
                assert(buf[i] == 0xff);
            }
        }

        addr += chunk_sz;
    }
}

static void
boot_test_util_verify_flash(const struct image_header *hdr1,
                            const struct image_header *hdr2)
{
    const struct ffs_sector_desc *sector_desc;
    int sector_idx;

    sector_idx = boot_test_img_sectors[0];

    while (1) {
        sector_desc = boot_test_sector_descs + sector_idx;
        if (sector_desc->fsd_offset == boot_test_img_addrs[1]) {
            break;
        }

        boot_test_util_verify_sector(sector_desc, hdr1,
                                     boot_test_img_addrs[0], 0);
        sector_idx++;
    }

    while (1) {
        if (sector_idx == BOOT_TEST_SECTOR_IDX_SCRATCH) {
            break;
        }

        sector_desc = boot_test_sector_descs + sector_idx;
        boot_test_util_verify_sector(sector_desc, hdr2,
                                     boot_test_img_addrs[1], 1);
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

    boot_test_util_verify_flash(&hdr, NULL);
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

    boot_test_util_verify_flash(&hdr, NULL);
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

    boot_test_util_verify_flash(&hdr0, &hdr1);
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

    rc = ffsutil_write_file("/boot/main", &hdr.ih_ver, sizeof hdr.ih_ver);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr, NULL);
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

    rc = ffsutil_write_file("/boot/main", &hdr.ih_ver, sizeof hdr.ih_ver);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr, NULL);
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

    rc = ffsutil_write_file("/boot/main", &hdr0.ih_ver, sizeof hdr0.ih_ver);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr0, sizeof hdr0) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr0, &hdr1);
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

    rc = ffsutil_write_file("/boot/main", &hdr1.ih_ver, sizeof hdr1.ih_ver);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr1, &hdr0);
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

    printf("\n");
}
