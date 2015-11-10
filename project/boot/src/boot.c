/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <util/flash_map.h>
#include <os/os.h>
#include "nffs/nffs.h"
#include "bootutil/image.h"
#include "bootutil/loader.h"

#define NFFS_AREA_MAX	16
#define SEC_CNT_MAX	8

/**
 * Boots the image described by the supplied image header.
 *
 * @param hdr                   The header for the image to boot.
 */
static void
boot_jump(const struct image_header *hdr, uint32_t image_addr)
{
    typedef void jump_fn(void);

    uint32_t base0entry;
    uint32_t img_start;
    uint32_t jump_addr;
    jump_fn *fn;

    /* PIC code not currently supported. */
    assert(!(hdr->ih_flags & IMAGE_F_PIC));

    img_start = image_addr + hdr->ih_hdr_size;

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

int
main(void)
{
    struct nffs_area_desc descs[NFFS_AREA_MAX];
    /** Contains indices of the areas which can contain image data. */
    struct flash_area secs[SEC_CNT_MAX];
    /** Areas representing the beginning of image slots. */
    uint8_t img_areas[NFFS_AREA_MAX];
    uint8_t img_starts[2];
    int cnt;
    struct boot_rsp rsp;
    int rc;
    int i, j;
    struct boot_req req = {
        .br_area_descs = descs,
        .br_image_areas = img_areas,
        .br_slot_areas = img_starts,
    };

    os_init();
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_0, &cnt, NULL);
    assert(rc == 0 && cnt < SEC_CNT_MAX);
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_0, &cnt, secs);
    img_starts[0] = 0;
    for (i = 0; i < cnt; i++) {
        img_areas[i] = i;
        descs[i].nad_flash_id = secs[i].fa_flash_id;
        descs[i].nad_offset = secs[i].fa_off;
        descs[i].nad_length = secs[i].fa_size;
    }
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_1, &cnt, secs);
    if (rc == 0 && cnt > 0) {
        img_starts[1] = i;
        for (j = 0; j < cnt; j++, i++) {
            img_areas[i] = i;
            descs[i].nad_flash_id = secs[j].fa_flash_id;
            descs[i].nad_offset = secs[j].fa_off;
            descs[i].nad_length = secs[j].fa_size;
        }
    }
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_SCRATCH, &cnt, secs);
    if (rc == 0) {
        for (j = 0; j < cnt; j++) {
            img_areas[i] = i;
            descs[i].nad_flash_id = secs[j].fa_flash_id;
            descs[i].nad_offset = secs[j].fa_off;
            descs[i].nad_length = secs[j].fa_size;
        }
    }
    req.br_num_image_areas = i;
    req.br_scratch_area_idx = i - 1;

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    boot_jump(rsp.br_hdr, rsp.br_image_addr);

    return 0;
}

