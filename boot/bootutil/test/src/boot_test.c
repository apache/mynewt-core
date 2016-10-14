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
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "testutil/testutil.h"
#include "hal/hal_flash.h"
#include "flash_map/flash_map.h"
#include "bootutil/image.h"
#include "bootutil/loader.h"
#include "bootutil/bootutil_misc.h"
#include "../src/bootutil_priv.h"

#include "mbedtls/sha256.h"

#define BOOT_TEST_HEADER_SIZE       0x200

/** Internal flash layout. */
static struct flash_area boot_test_area_descs[] = {
    [0] = { .fa_off = 0x00020000, .fa_size = 128 * 1024 },
    [1] = { .fa_off = 0x00040000, .fa_size = 128 * 1024 },
    [2] = { .fa_off = 0x00060000, .fa_size = 128 * 1024 },
    [3] = { .fa_off = 0x00080000, .fa_size = 128 * 1024 },
    [4] = { .fa_off = 0x000a0000, .fa_size = 128 * 1024 },
    [5] = { .fa_off = 0x000c0000, .fa_size = 128 * 1024 },
    [6] = { .fa_off = 0x000e0000, .fa_size = 128 * 1024 },
    [7] = { 0 },
};

/** Areas representing the beginning of image slots. */
static uint8_t boot_test_slot_areas[] = {
    0, 3,
};

/** Flash offsets of the two image slots. */
static struct {
    uint8_t flash_id;
    uint32_t address;
} boot_test_img_addrs[] = {
    { 0, 0x20000 },
    { 0, 0x80000 },
};

/** Three areas per image slot */
#define BOOT_TEST_IMAGE_NUM_AREAS  3

#define BOOT_TEST_AREA_IDX_SCRATCH 6

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
    const struct flash_area *area_desc;
    int rc;

    rc = hal_flash_init();
    TEST_ASSERT(rc == 0);

    for (area_desc = boot_test_area_descs;
         area_desc->fa_size != 0;
         area_desc++) {

        rc = flash_area_erase(area_desc, 0, area_desc->fa_size);
        TEST_ASSERT(rc == 0);
    }
}

static void
boot_test_util_copy_area(int from_area_idx, int to_area_idx)
{
    const struct flash_area *from_area_desc;
    const struct flash_area *to_area_desc;
    void *buf;
    int rc;

    from_area_desc = boot_test_area_descs + from_area_idx;
    to_area_desc = boot_test_area_descs + to_area_idx;

    TEST_ASSERT(from_area_desc->fa_size == to_area_desc->fa_size);

    buf = malloc(from_area_desc->fa_size);
    TEST_ASSERT(buf != NULL);

    rc = flash_area_read(from_area_desc, 0, buf,
                         from_area_desc->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_erase(to_area_desc,
                          0,
                          to_area_desc->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_write(to_area_desc, 0, buf,
                          to_area_desc->fa_size);
    TEST_ASSERT(rc == 0);

    free(buf);
}

static uint32_t
boot_test_util_area_write_size(int dst_idx, uint32_t off, uint32_t size)
{
    const struct flash_area *desc;
    int64_t diff;
    uint32_t trailer_start;
    int i;

    for (i = 0;
         i < sizeof boot_test_slot_areas / sizeof boot_test_slot_areas[0];
         i++) {

        /* Don't include trailer in copy. */
        if (dst_idx ==
            boot_test_slot_areas[i] + BOOT_TEST_IMAGE_NUM_AREAS - 1) {

            desc = boot_test_area_descs + dst_idx;
            trailer_start = desc->fa_size - boot_status_sz();
            diff = off + size - trailer_start;
            if (diff > 0) {
                if (diff > size) {
                    size = 0;
                } else {
                    size -= diff;
                }
            }

            break;
        }
    }

    return size;
}

static void
boot_test_util_swap_areas(int area_idx1, int area_idx2)
{
    const struct flash_area *area_desc1;
    const struct flash_area *area_desc2;
    uint32_t size;
    void *buf1;
    void *buf2;
    int rc;

    area_desc1 = boot_test_area_descs + area_idx1;
    area_desc2 = boot_test_area_descs + area_idx2;

    TEST_ASSERT(area_desc1->fa_size == area_desc2->fa_size);

    buf1 = malloc(area_desc1->fa_size);
    TEST_ASSERT(buf1 != NULL);

    buf2 = malloc(area_desc2->fa_size);
    TEST_ASSERT(buf2 != NULL);

    rc = flash_area_read(area_desc1, 0, buf1, area_desc1->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_read(area_desc2, 0, buf2, area_desc2->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_erase(area_desc1, 0, area_desc1->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_erase(area_desc2, 0, area_desc2->fa_size);
    TEST_ASSERT(rc == 0);

    size = boot_test_util_area_write_size(area_idx1, 0, area_desc1->fa_size);
    rc = flash_area_write(area_desc1, 0, buf2, size);
    TEST_ASSERT(rc == 0);

    size = boot_test_util_area_write_size(area_idx2, 0, area_desc2->fa_size);
    rc = flash_area_write(area_desc2, 0, buf1, size);
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
boot_test_util_verify_area(const struct flash_area *area_desc,
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

    addr = area_desc->fa_off;

    if (hdr != NULL) {
        img_size = hdr->ih_img_size;

        if (addr == image_addr) {
            rc = hal_flash_read(area_desc->fa_device_id, image_addr,
                                &temp_hdr, sizeof temp_hdr);
            TEST_ASSERT(rc == 0);
            TEST_ASSERT(memcmp(&temp_hdr, hdr, sizeof *hdr) == 0);

            addr += hdr->ih_hdr_size;
        }
    } else {
        img_size = 0;
    }

    area_end = area_desc->fa_off + area_desc->fa_size;
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

        rc = hal_flash_read(area_desc->fa_device_id, addr, buf, chunk_sz);
        TEST_ASSERT(rc == 0);

        for (i = 0; i < chunk_sz; i++) {
            if (rem_img > 0) {
                TEST_ASSERT(buf[i] == boot_test_util_byte_at(img_msb,
                                                        img_off + i));
            } else if (past_image) {
#if 0
                TEST_ASSERT(buf[i] == 0xff);
#endif
            }
        }

        addr += chunk_sz;
    }
}

static void
boot_test_util_verify_status_clear(void)
{
    struct boot_img_trailer bit;
    const struct flash_area *fap;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    TEST_ASSERT(rc == 0);

    rc = flash_area_read(fap, fap->fa_size - sizeof(bit), &bit, sizeof(bit));
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(bit.bit_copy_start != BOOT_MAGIC_SWAP_TEMP ||
      bit.bit_copy_done != 0xff);
}

static void
boot_test_util_verify_flash(const struct image_header *hdr0, int orig_slot_0,
                            const struct image_header *hdr1, int orig_slot_1)
{
    const struct flash_area *area_desc;
    int area_idx;

    area_idx = 0;

    while (1) {
        area_desc = boot_test_area_descs + area_idx;
        if (area_desc->fa_off == boot_test_img_addrs[1].address &&
            area_desc->fa_device_id == boot_test_img_addrs[1].flash_id) {
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

static void
boot_test_util_verify_all(const struct boot_req *req, int expected_swap_type,
                          const struct image_header *hdr0,
                          const struct image_header *hdr1)
{
    const struct image_header *slot0hdr;
    const struct image_header *slot1hdr;
    struct boot_rsp rsp;
    int orig_slot_0;
    int orig_slot_1;
    int num_swaps;
    int rc;
    int i;

    TEST_ASSERT_FATAL(req != NULL);
    TEST_ASSERT_FATAL(hdr0 != NULL || hdr1 != NULL);

    num_swaps = 0;
    for (i = 0; i < 3; i++) {
        rc = boot_go(req, &rsp);
        TEST_ASSERT_FATAL(rc == 0);

        if (expected_swap_type != BOOT_SWAP_TYPE_NONE) {
            num_swaps++;
        }

        if (num_swaps % 2 == 0) {
            if (hdr0 != NULL) {
                slot0hdr = hdr0;
                slot1hdr = hdr1;
            } else {
                slot0hdr = hdr1;
                slot1hdr = hdr0;
            }
            orig_slot_0 = 0;
            orig_slot_1 = 1;
        } else {
            if (hdr1 != NULL) {
                slot0hdr = hdr1;
                slot1hdr = hdr0;
            } else {
                slot0hdr = hdr0;
                slot1hdr = hdr1;
            }
            orig_slot_0 = 1;
            orig_slot_1 = 0;
        }

        TEST_ASSERT(memcmp(rsp.br_hdr, slot0hdr, sizeof *slot0hdr) == 0);
        TEST_ASSERT(rsp.br_flash_id == boot_test_img_addrs[0].flash_id);
        TEST_ASSERT(rsp.br_image_addr == boot_test_img_addrs[0].address);

        boot_test_util_verify_flash(slot0hdr, orig_slot_0,
                                    slot1hdr, orig_slot_1);
        boot_test_util_verify_status_clear();

        if (expected_swap_type != BOOT_SWAP_TYPE_NONE) {
            switch (expected_swap_type) {
            case BOOT_SWAP_TYPE_TEMP:
                expected_swap_type = BOOT_SWAP_TYPE_PERM;
                break;

            case BOOT_SWAP_TYPE_PERM:
                expected_swap_type = BOOT_SWAP_TYPE_NONE;
                break;

            default:
                TEST_ASSERT_FATAL(0);
                break;
            }
        }
    }
}

TEST_CASE(boot_test_nv_ns_10)
{
    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    boot_test_util_write_hash(&hdr, 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_NONE, &hdr, NULL);
}

TEST_CASE(boot_test_nv_ns_01)
{
    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 10 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 1);
    boot_test_util_write_hash(&hdr, 1);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_PERM, NULL, &hdr);
}

TEST_CASE(boot_test_nv_ns_11)
{
    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_NONE, &hdr0, &hdr1);
}

TEST_CASE(boot_test_vm_ns_10)
{
    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    boot_test_util_write_hash(&hdr, 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_NONE, &hdr, NULL);
}

TEST_CASE(boot_test_vm_ns_01)
{
    int rc;

    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 10 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 1);
    boot_test_util_write_hash(&hdr, 1);

    rc = boot_vect_write_test(1);
    TEST_ASSERT(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_PERM, NULL, &hdr);
}

TEST_CASE(boot_test_vm_ns_11_a)
{
    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_NONE, &hdr0, &hdr1);
}

TEST_CASE(boot_test_vm_ns_11_b)
{
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    rc = boot_vect_write_test(1);
    TEST_ASSERT(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_TEMP, &hdr0, &hdr1);
}

TEST_CASE(boot_test_vm_ns_11_2areas)
{
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 196 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    rc = boot_vect_write_test(1);
    TEST_ASSERT(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_TEMP, &hdr0, &hdr1);
}

TEST_CASE(boot_test_nv_bs_10)
{
    struct image_header hdr = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr, 0);
    boot_test_util_write_hash(&hdr, 0);
    boot_test_util_swap_areas(boot_test_slot_areas[1],
      BOOT_TEST_AREA_IDX_SCRATCH);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_NONE, &hdr, NULL);
}

TEST_CASE(boot_test_nv_bs_11)
{
    struct boot_status status;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 17 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 1, 5, 5 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);
    rc = boot_vect_write_test(1);
    boot_test_util_copy_area(5, BOOT_TEST_AREA_IDX_SCRATCH);

    boot_req_set(&req);
    status.idx = 0;
    status.elem_sz = 1;
    status.state = 1;

    rc = boot_write_status(&status);
    TEST_ASSERT(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_TEMP, &hdr0, &hdr1);
}

TEST_CASE(boot_test_nv_bs_11_2areas)
{
    struct boot_status status;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 150 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 190 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    boot_test_util_swap_areas(2, 5);

    rc = boot_vect_write_test(1);
    TEST_ASSERT_FATAL(rc == 0);

    status.idx = 1;
    status.elem_sz = 1;
    status.state = 0;

    rc = boot_write_status(&status);
    TEST_ASSERT_FATAL(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_TEMP, &hdr0, &hdr1);
}

TEST_CASE(boot_test_vb_ns_11)
{
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    rc = boot_vect_write_test(1);
    TEST_ASSERT_FATAL(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_TEMP, &hdr0, &hdr1);
}

TEST_CASE(boot_test_no_hash)
{
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };
    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 0,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);

    rc = boot_vect_write_test(1);
    TEST_ASSERT_FATAL(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_NONE, &hdr0, NULL);
}

TEST_CASE(boot_test_no_flag_has_hash)
{
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };
    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    rc = boot_vect_write_test(1);
    TEST_ASSERT(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_NONE, &hdr0, NULL);
}

TEST_CASE(boot_test_invalid_hash)
{
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };
    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    struct image_tlv tlv = {
        .it_type = IMAGE_TLV_SHA256,
        .it_len = 32
    };
    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    rc = hal_flash_write(boot_test_img_addrs[1].flash_id,
      boot_test_img_addrs[1].address + hdr1.ih_hdr_size + hdr1.ih_img_size,
      &tlv, sizeof(tlv));
    TEST_ASSERT(rc == 0);

    rc = boot_vect_write_test(1);
    TEST_ASSERT(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_NONE, &hdr0, NULL);
}

TEST_CASE(boot_test_revert)
{
    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    /* Indicate that the image in slot 0 is being tested. */
    boot_set_copy_done();

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_PERM, &hdr0, &hdr1);
}

TEST_CASE(boot_test_revert_continue)
{
    struct boot_status status;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct boot_req req = {
        .br_area_descs = boot_test_area_descs,
        .br_slot_areas = boot_test_slot_areas,
        .br_num_image_areas = BOOT_TEST_AREA_IDX_SCRATCH + 1,
        .br_scratch_area_idx = BOOT_TEST_AREA_IDX_SCRATCH,
        .br_img_sz = (384 * 1024),
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    boot_test_util_swap_areas(2, 5);

    /* Indicate that the image in slot 0 is being tested. */
    boot_set_copy_done();

    status.idx = 1;
    status.elem_sz = 1;
    status.state = 0;

    rc = boot_write_status(&status);
    TEST_ASSERT_FATAL(rc == 0);

    boot_test_util_verify_all(&req, BOOT_SWAP_TYPE_PERM, &hdr0, &hdr1);
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
    boot_test_revert();
    boot_test_revert_continue();
}

int
boot_test_all(void)
{
    boot_test_main();
    return tu_any_failed;
}

#if MYNEWT_VAL(SELFTEST)

int
main(int argc, char **argv)
{
    tu_config.tc_print_results = 1;
    tu_parse_args(argc, argv);

    tu_init();

    boot_test_all();

    return tu_any_failed;
}

#endif
