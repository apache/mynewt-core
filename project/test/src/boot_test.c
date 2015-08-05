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

/** Internal flash layout. */
static struct ffs_area_desc boot_test_area_descs[] = {
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

static const struct ffs_area_desc boot_test_format_descs[] = {
    [0] =  { 0x00004000, 16 * 1024 },
    [1] =  { 0x00008000, 16 * 1024 },
    [2] =  { 0x0000c000, 16 * 1024 },
    { 0, 0 },
};

/** Contains indices of the areas which can contain image data. */
static uint16_t boot_test_img_areas[] = {
    5, 6, 7, 8, 9, 10, 11
};

/** Areas representing the beginning of image slots. */
static uint16_t boot_test_slot_areas[] = {
    5, 8,
};

/** Flash offsets of the two image slots. */
static uint32_t boot_test_img_addrs[] = {
    0x20000,
    0x80000,
};

#define BOOT_TEST_NUM_IMG_AREAS \
    ((int)(sizeof boot_test_img_areas / sizeof boot_test_img_areas[0]))

#define BOOT_TEST_AREA_IDX_SCRATCH 11
#define BOOT_TEST_IMG_AREA_IDX_SCRATCH 6

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
    const struct ffs_area_desc *area_desc;
    int rc;

    rc = flash_init();
    assert(rc == 0);

    for (area_desc = boot_test_area_descs;
         area_desc->fad_length != 0;
         area_desc++) {

        rc = flash_erase(area_desc->fad_offset, area_desc->fad_length);
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
boot_test_util_copy_area(int from_area_idx, int to_area_idx)
{
    const struct ffs_area_desc *from_area_desc;
    const struct ffs_area_desc *to_area_desc;
    void *buf;
    int rc;

    from_area_desc = boot_test_area_descs + from_area_idx;
    to_area_desc = boot_test_area_descs + to_area_idx;

    assert(from_area_desc->fad_length == to_area_desc->fad_length);

    buf = malloc(from_area_desc->fad_length);
    assert(buf != NULL);

    rc = flash_read(from_area_desc->fad_offset, buf,
                    from_area_desc->fad_length);
    assert(rc == 0);

    rc = flash_erase(to_area_desc->fad_offset, to_area_desc->fad_length);
    assert(rc == 0);

    rc = flash_write(to_area_desc->fad_offset, buf,
                     to_area_desc->fad_length);
    assert(rc == 0);

    free(buf);
}

static void
boot_test_util_swap_areas(int area_idx1, int area_idx2)
{
    const struct ffs_area_desc *area_desc1;
    const struct ffs_area_desc *area_desc2;
    void *buf1;
    void *buf2;
    int rc;

    area_desc1 = boot_test_area_descs + area_idx1;
    area_desc2 = boot_test_area_descs + area_idx2;

    assert(area_desc1->fad_length == area_desc2->fad_length);

    buf1 = malloc(area_desc1->fad_length);
    assert(buf1 != NULL);

    buf2 = malloc(area_desc2->fad_length);
    assert(buf2 != NULL);

    rc = flash_read(area_desc1->fad_offset, buf1, area_desc1->fad_length);
    assert(rc == 0);

    rc = flash_read(area_desc2->fad_offset, buf2, area_desc2->fad_length);
    assert(rc == 0);

    rc = flash_erase(area_desc1->fad_offset, area_desc1->fad_length);
    assert(rc == 0);

    rc = flash_erase(area_desc2->fad_offset, area_desc2->fad_length);
    assert(rc == 0);

    rc = flash_write(area_desc1->fad_offset, buf2, area_desc1->fad_length);
    assert(rc == 0);

    rc = flash_write(area_desc2->fad_offset, buf1, area_desc2->fad_length);
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

    rc = flash_write(off, hdr, sizeof *hdr);
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

        rc = flash_write(off + image_off, buf, chunk_sz);
        assert(rc == 0);

        image_off += chunk_sz;
    }
}

static void
boot_test_util_verify_area(const struct ffs_area_desc *area_desc,
                           const struct image_header *hdr,
                           uint32_t image_addr, int img_msb)
{
    struct image_header temp_hdr;
    uint32_t area_end;
    uint32_t img_size;
    uint32_t img_off;
    uint32_t img_end;
    uint32_t addr;
    uint8_t buf[256];
    int rem_area;
    int past_image;
    int chunk_sz;
    int rem_img;
    int rc;
    int i;

    addr = area_desc->fad_offset;

    if (hdr != NULL) {
        img_size = hdr->ih_img_size;

        if (addr == image_addr) {
            rc = flash_read(image_addr, &temp_hdr, sizeof temp_hdr);
            assert(rc == 0);
            assert(memcmp(&temp_hdr, hdr, sizeof *hdr) == 0);

            addr += hdr->ih_hdr_size;
        }
    } else {
        img_size = 0;
    }

    area_end = area_desc->fad_offset + area_desc->fad_length;
    img_end = image_addr + img_size;
    past_image = addr >= img_end;

    while (addr < area_end) {
        rem_area = area_end - addr;
        rem_img = img_end - addr;

        if (hdr != NULL) {
            img_off = addr - image_addr - hdr->ih_hdr_size;
        }

        if (rem_area > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = rem_area;
        }

        rc = flash_read(addr, buf, chunk_sz);
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
    const struct ffs_area_desc *area_desc;
    int area_idx;

    area_idx = boot_test_img_areas[0];

    while (1) {
        area_desc = boot_test_area_descs + area_idx;
        if (area_desc->fad_offset == boot_test_img_addrs[1]) {
            break;
        }

        boot_test_util_verify_area(area_desc, hdr0, boot_test_img_addrs[0],
                                   orig_slot_0);
        area_idx++;
    }

    while (1) {
        if (area_idx == BOOT_TEST_AREA_IDX_SCRATCH) {
            break;
        }

        area_desc = boot_test_area_descs + area_idx;
        boot_test_util_verify_area(area_desc, hdr1, boot_test_img_addrs[1],
                                   orig_slot_1);
        area_idx++;
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
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
boot_test_vm_ns_11_2areas(void)
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    printf("\tvector-main no-status 1-1-2areas test\n");

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
    struct boot_status_entry entries[BOOT_TEST_NUM_IMG_AREAS];
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    printf("\tno-vector basic-status 1-0 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    boot_test_util_swap_areas(boot_test_img_areas[0],
                                BOOT_TEST_AREA_IDX_SCRATCH);

    memset(&status, 0xff, sizeof status);
    status.bs_img2_length = hdr.ih_img_size;

    memset(entries, 0xff, sizeof entries);
    entries[BOOT_TEST_IMG_AREA_IDX_SCRATCH].bse_image_num = 1;
    entries[BOOT_TEST_IMG_AREA_IDX_SCRATCH].bse_part_num = 0;

    rc = boot_write_status(&status, entries, BOOT_TEST_NUM_IMG_AREAS);
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
    struct boot_status_entry entries[BOOT_TEST_NUM_IMG_AREAS];
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    printf("\tno-vector basic-status 1-1 test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_copy_area(boot_test_img_areas[0],
                             BOOT_TEST_AREA_IDX_SCRATCH);

    memset(&status, 0xff, sizeof status);
    status.bs_img1_length = hdr0.ih_img_size;
    status.bs_img2_length = hdr1.ih_img_size;

    memset(entries, 0xff, sizeof entries);
    entries[3].bse_image_num = 1;
    entries[3].bse_part_num = 0;
    entries[BOOT_TEST_IMG_AREA_IDX_SCRATCH].bse_image_num = 0;
    entries[BOOT_TEST_IMG_AREA_IDX_SCRATCH].bse_part_num = 0;

    rc = boot_write_status(&status, entries, BOOT_TEST_NUM_IMG_AREAS);
    assert(rc == 0);

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    assert(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    assert(rsp.br_image_addr == boot_test_img_addrs[0]);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();
}

static void
boot_test_nv_bs_11_2areas(void)
{
    struct boot_status_entry entries[BOOT_TEST_NUM_IMG_AREAS];
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    printf("\tno-vector basic-status 1-1-2areas test\n");

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_swap_areas(boot_test_img_areas[0], boot_test_img_areas[3]);

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

    rc = boot_write_status(&status, entries, BOOT_TEST_NUM_IMG_AREAS);
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
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
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
    boot_test_vm_ns_11_2areas();
    boot_test_nv_bs_10();
    boot_test_nv_bs_11();
    boot_test_nv_bs_11_2areas();
    boot_test_vb_ns_11();

    printf("\n");
}
