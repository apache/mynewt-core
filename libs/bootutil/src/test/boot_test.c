/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "testutil/testutil.h"
#include "hal/hal_flash.h"
#include "fs/fs.h"
#include "fs/fsutil.h"
#include "nffs/nffs.h"
#include "bootutil/image.h"
#include "bootutil/loader.h"
#include "../src/bootutil_priv.h"

#include "mbedtls/sha256.h"

#define BOOT_TEST_HEADER_SIZE       0x200

/** Internal flash layout. */
static struct nffs_area_desc boot_test_area_descs[] = {
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

static const struct nffs_area_desc boot_test_format_descs[] = {
    [0] =  { 0x00004000, 16 * 1024 },
    [1] =  { 0x00008000, 16 * 1024 },
    [2] =  { 0x0000c000, 16 * 1024 },
    { 0, 0 },
};

/** Contains indices of the areas which can contain image data. */
static uint8_t boot_test_img_areas[] = {
    5, 6, 7, 8, 9, 10, 11
};

/** Areas representing the beginning of image slots. */
static uint8_t boot_test_slot_areas[] = {
    5, 8,
};

/** Flash offsets of the two image slots. */
static struct {
    uint8_t flash_id;
    uint32_t address;
} boot_test_img_addrs[] = {
    { 0, 0x20000 },
    { 0, 0x80000 },
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

    TEST_ASSERT(image_offset < 0x01000000);
    u32 = image_offset + (img_msb << 24);
    u8p = (void *)&u32;
    return u8p[image_offset % 4];
}

static void
boot_test_util_init_flash(void)
{
    const struct nffs_area_desc *area_desc;
    int rc;

    rc = hal_flash_init();
    TEST_ASSERT(rc == 0);

    for (area_desc = boot_test_area_descs;
         area_desc->nad_length != 0;
         area_desc++) {

        rc = hal_flash_erase(area_desc->nad_flash_id, area_desc->nad_offset,
                             area_desc->nad_length);
        TEST_ASSERT(rc == 0);
    }

    rc = nffs_init();
    TEST_ASSERT(rc == 0);

    rc = nffs_format(boot_test_format_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/boot");
    TEST_ASSERT(rc == 0);
}

static void
boot_test_util_copy_area(int from_area_idx, int to_area_idx)
{
    const struct nffs_area_desc *from_area_desc;
    const struct nffs_area_desc *to_area_desc;
    void *buf;
    int rc;

    from_area_desc = boot_test_area_descs + from_area_idx;
    to_area_desc = boot_test_area_descs + to_area_idx;

    TEST_ASSERT(from_area_desc->nad_length == to_area_desc->nad_length);

    buf = malloc(from_area_desc->nad_length);
    TEST_ASSERT(buf != NULL);

    rc = hal_flash_read(from_area_desc->nad_flash_id,
                        from_area_desc->nad_offset, buf,
                        from_area_desc->nad_length);
    TEST_ASSERT(rc == 0);

    rc = hal_flash_erase(to_area_desc->nad_flash_id,
                         to_area_desc->nad_offset,
                         to_area_desc->nad_length);
    TEST_ASSERT(rc == 0);

    rc = hal_flash_write(to_area_desc->nad_flash_id,
                         to_area_desc->nad_offset, buf,
                         to_area_desc->nad_length);
    TEST_ASSERT(rc == 0);

    free(buf);
}

static void
boot_test_util_swap_areas(int area_idx1, int area_idx2)
{
    const struct nffs_area_desc *area_desc1;
    const struct nffs_area_desc *area_desc2;
    void *buf1;
    void *buf2;
    int rc;

    area_desc1 = boot_test_area_descs + area_idx1;
    area_desc2 = boot_test_area_descs + area_idx2;

    TEST_ASSERT(area_desc1->nad_length == area_desc2->nad_length);

    buf1 = malloc(area_desc1->nad_length);
    TEST_ASSERT(buf1 != NULL);

    buf2 = malloc(area_desc2->nad_length);
    TEST_ASSERT(buf2 != NULL);

    rc = hal_flash_read(area_desc1->nad_flash_id, area_desc1->nad_offset,
                        buf1, area_desc1->nad_length);
    TEST_ASSERT(rc == 0);

    rc = hal_flash_read(area_desc2->nad_flash_id, area_desc2->nad_offset,
                        buf2, area_desc2->nad_length);
    TEST_ASSERT(rc == 0);

    rc = hal_flash_erase(area_desc1->nad_flash_id, area_desc1->nad_offset,
                         area_desc1->nad_length);
    TEST_ASSERT(rc == 0);

    rc = hal_flash_erase(area_desc2->nad_flash_id, area_desc2->nad_offset,
                         area_desc2->nad_length);
    TEST_ASSERT(rc == 0);

    rc = hal_flash_write(area_desc1->nad_flash_id, area_desc1->nad_offset,
                         buf2, area_desc1->nad_length);
    TEST_ASSERT(rc == 0);

    rc = hal_flash_write(area_desc2->nad_flash_id, area_desc2->nad_offset,
                         buf1, area_desc2->nad_length);
    TEST_ASSERT(rc == 0);

    free(buf1);
    free(buf2);
}

static void
boot_test_util_write_image(const struct image_header *hdr, int slot)
{
    uint32_t image_off;
    uint32_t off;
    uint8_t flash_id;
    uint8_t buf[256];
    int chunk_sz;
    int rc;
    int i;

    TEST_ASSERT(slot == 0 || slot == 1);

    flash_id = boot_test_img_addrs[slot].flash_id;
    off = boot_test_img_addrs[slot].address;

    rc = hal_flash_write(flash_id, off, hdr, sizeof *hdr);
    TEST_ASSERT(rc == 0);

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

        rc = hal_flash_write(flash_id, off + image_off, buf, chunk_sz);
        TEST_ASSERT(rc == 0);

        image_off += chunk_sz;
    }
}

static void
boot_test_util_write_hash(const struct image_header *hdr, int slot)
{
    uint8_t tmpdata[1024];
    uint8_t hash[32];
    int rc;
    uint32_t off;
    uint32_t blk_sz;
    uint32_t sz;
    mbedtls_sha256_context ctx;
    uint8_t flash_id;
    uint32_t addr;
    struct image_tlv tlv;

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    flash_id = boot_test_img_addrs[slot].flash_id;
    addr = boot_test_img_addrs[slot].address;

    sz = hdr->ih_hdr_size + hdr->ih_img_size;
    for (off = 0; off < sz; off += blk_sz) {
        blk_sz = sz - off;
        if (blk_sz > sizeof(tmpdata)) {
            blk_sz = sizeof(tmpdata);
        }
        rc = hal_flash_read(flash_id, addr + off, tmpdata, blk_sz);
        TEST_ASSERT(rc == 0);
        mbedtls_sha256_update(&ctx, tmpdata, blk_sz);
    }
    mbedtls_sha256_finish(&ctx, hash);

    tlv.it_type = IMAGE_TLV_SHA256;
    tlv._pad = 0;
    tlv.it_len = sizeof(hash);

    rc = hal_flash_write(flash_id, addr + off, &tlv, sizeof(tlv));
    TEST_ASSERT(rc == 0);
    off += sizeof(tlv);
    rc = hal_flash_write(flash_id, addr + off, hash, sizeof(hash));
    TEST_ASSERT(rc == 0);
}

static void
boot_test_util_verify_area(const struct nffs_area_desc *area_desc,
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

    addr = area_desc->nad_offset;

    if (hdr != NULL) {
        img_size = hdr->ih_img_size;

        if (addr == image_addr) {
            rc = hal_flash_read(area_desc->nad_flash_id, image_addr,
                                &temp_hdr, sizeof temp_hdr);
            TEST_ASSERT(rc == 0);
            TEST_ASSERT(memcmp(&temp_hdr, hdr, sizeof *hdr) == 0);

            addr += hdr->ih_hdr_size;
        }
    } else {
        img_size = 0;
    }

    area_end = area_desc->nad_offset + area_desc->nad_length;
    img_end = image_addr + img_size;
    past_image = addr >= img_end;

    while (addr < area_end) {
        rem_area = area_end - addr;
        rem_img = img_end - addr;

        if (hdr != NULL) {
            img_off = addr - image_addr - hdr->ih_hdr_size;
        } else {
            img_off = 0;
        }

        if (rem_area > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = rem_area;
        }

        rc = hal_flash_read(area_desc->nad_flash_id, addr, buf, chunk_sz);
        TEST_ASSERT(rc == 0);

        for (i = 0; i < chunk_sz; i++) {
            if (rem_img > 0) {
                TEST_ASSERT(buf[i] == boot_test_util_byte_at(img_msb,
                                                        img_off + i));
            } else if (past_image) {
                TEST_ASSERT(buf[i] == 0xff);
            }
        }

        addr += chunk_sz;
    }
}

static void
boot_test_util_verify_status_clear(void)
{
    struct fs_file *file;
    int rc;

    rc = fs_open(BOOT_PATH_STATUS, FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_ENOENT);
}

static void
boot_test_util_verify_flash(const struct image_header *hdr0, int orig_slot_0,
                            const struct image_header *hdr1, int orig_slot_1)
{
    const struct nffs_area_desc *area_desc;
    int area_idx;

    area_idx = boot_test_img_areas[0];

    while (1) {
        area_desc = boot_test_area_descs + area_idx;
        if (area_desc->nad_offset == boot_test_img_addrs[1].address &&
            area_desc->nad_flash_id == boot_test_img_addrs[1].flash_id) {
            break;
        }

        boot_test_util_verify_area(area_desc, hdr0,
                                   boot_test_img_addrs[0].address, orig_slot_0);
        area_idx++;
    }

    while (1) {
        if (area_idx == BOOT_TEST_AREA_IDX_SCRATCH) {
            break;
        }

        area_desc = boot_test_area_descs + area_idx;
        boot_test_util_verify_area(area_desc, hdr1,
                                   boot_test_img_addrs[1].address, orig_slot_1);
        area_idx++;
    }
}

TEST_CASE(boot_test_nv_ns_10)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    boot_test_util_write_hash(&hdr, 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr, 0, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_nv_ns_01)
{
    struct boot_rsp rsp;
    int rc;


    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 10 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 1);
    boot_test_util_write_hash(&hdr, 1);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr, 1, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_nv_ns_11)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr0, sizeof hdr0) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr0, 0, &hdr1, 1);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_vm_ns_10)
{
    struct boot_rsp rsp;
    int rc;


    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    boot_test_util_write_hash(&hdr, 0);

    rc = fsutil_write_file(BOOT_PATH_MAIN, &hdr.ih_ver, sizeof hdr.ih_ver);
    TEST_ASSERT(rc == 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr, 0, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_vm_ns_01)
{
    struct boot_rsp rsp;
    int rc;


    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 10 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 1);
    boot_test_util_write_hash(&hdr, 1);

    rc = fsutil_write_file(BOOT_PATH_MAIN, &hdr.ih_ver, sizeof hdr.ih_ver);
    TEST_ASSERT(rc == 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr, 1, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_vm_ns_11_a)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    rc = fsutil_write_file(BOOT_PATH_MAIN, &hdr0.ih_ver, sizeof hdr0.ih_ver);
    TEST_ASSERT(rc == 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr0, sizeof hdr0) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr0, 0, &hdr1, 1);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_vm_ns_11_b)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    rc = fsutil_write_file(BOOT_PATH_MAIN, &hdr1.ih_ver, sizeof hdr1.ih_ver);
    TEST_ASSERT(rc == 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_vm_ns_11_2areas)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 196 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    rc = fsutil_write_file(BOOT_PATH_MAIN, &hdr1.ih_ver, sizeof hdr1.ih_ver);
    TEST_ASSERT(rc == 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_nv_bs_10)
{
    struct boot_status_entry entries[BOOT_TEST_NUM_IMG_AREAS];
    struct boot_status status;
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    boot_test_util_write_hash(&hdr, 0);
    boot_test_util_swap_areas(boot_test_img_areas[0],
                                BOOT_TEST_AREA_IDX_SCRATCH);

    memset(&status, 0xff, sizeof status);
    status.bs_img2_length = hdr.ih_img_size;

    memset(entries, 0xff, sizeof entries);
    entries[BOOT_TEST_IMG_AREA_IDX_SCRATCH].bse_image_num = 1;
    entries[BOOT_TEST_IMG_AREA_IDX_SCRATCH].bse_part_num = 0;

    rc = boot_write_status(&status, entries, BOOT_TEST_NUM_IMG_AREAS);
    TEST_ASSERT(rc == 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr, sizeof hdr) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr, 0, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_nv_bs_11)
{
    struct boot_status_entry entries[BOOT_TEST_NUM_IMG_AREAS];
    struct boot_status status;
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 17 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 1, 1, 5, 5 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);
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
    TEST_ASSERT(rc == 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_nv_bs_11_2areas)
{
    struct boot_status_entry entries[BOOT_TEST_NUM_IMG_AREAS];
    struct boot_status status;
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 150 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 190 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);
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
    TEST_ASSERT(rc == 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_vb_ns_11)
{
    struct boot_rsp rsp;
    int rc;
    int i;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_hash(&hdr1, 1);

    rc = fsutil_write_file(BOOT_PATH_MAIN, &hdr0.ih_ver, sizeof hdr0.ih_ver);
    TEST_ASSERT(rc == 0);

    rc = fsutil_write_file(BOOT_PATH_TEST, &hdr1.ih_ver, sizeof hdr1.ih_ver);
    TEST_ASSERT(rc == 0);

    /* First boot should use the test image. */
    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(memcmp(rsp.br_hdr, &hdr1, sizeof hdr1) == 0);
    TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
    TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

    boot_test_util_verify_flash(&hdr1, 1, &hdr0, 0);
    boot_test_util_verify_status_clear();

    /* Ensure all subsequent boots use the main image. */
    for (i = 0; i < 10; i++) {
        rc = boot_go(&req, &rsp);
        TEST_ASSERT(rc == 0);

        TEST_ASSERT(memcmp(rsp.br_hdr, &hdr0, sizeof hdr0) == 0);
        TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
        TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

        boot_test_util_verify_flash(&hdr0, 0, &hdr1, 1);
        boot_test_util_verify_status_clear();
    }
}

TEST_CASE(boot_test_no_hash)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 0,
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

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc != 0);

    boot_test_util_verify_flash(&hdr, 0, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_no_flag_has_hash)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
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

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    boot_test_util_write_hash(&hdr, 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc != 0);

    boot_test_util_verify_flash(&hdr, 0, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

TEST_CASE(boot_test_invalid_hash)
{
    struct boot_rsp rsp;
    int rc;

    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_HAS_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_image_areas = boot_test_img_areas,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_NUM_IMG_AREAS,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
    };

    struct image_tlv tlv = {
        .it_type = IMAGE_TLV_SHA256,
        .it_len = 32
    };
    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    rc = hal_flash_write(boot_test_img_addrs[0].flash_id,
      boot_test_img_addrs[0].address + hdr.ih_hdr_size + hdr.ih_img_size,
      &tlv, sizeof(tlv));
    TEST_ASSERT(rc == 0);

    rc = boot_go(&req, &rsp);
    TEST_ASSERT(rc != 0);

    boot_test_util_verify_flash(&hdr, 0, NULL, 0xff);
    boot_test_util_verify_status_clear();
}

TEST_SUITE(boot_test_main)
{
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
    boot_test_no_hash();
    boot_test_no_flag_has_hash();
    boot_test_invalid_hash();
}

int
boot_test_all(void)
{
    boot_test_main();
    return tu_any_failed;
}

#ifdef MYNEWT_SELFTEST

int
main(void)
{
    tu_config.tc_print_results = 1;
    tu_init();

    boot_test_all();

    return tu_any_failed;
}

#endif
