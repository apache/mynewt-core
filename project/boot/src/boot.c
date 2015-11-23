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
    uint8_t img_areas[NFFS_AREA_MAX];
    /** Areas representing the beginning of image slots. */
    uint8_t img_starts[2];
    int cnt;
    int total;
    struct boot_rsp rsp;
    int rc;
    struct boot_req req = {
        .br_area_descs = descs,
        .br_image_areas = img_areas,
        .br_slot_areas = img_starts,
    };

    os_init();
    rc = flash_area_to_nffs_desc(FLASH_AREA_IMAGE_0, &cnt, NULL);
    assert(rc == 0 && cnt);
    rc = flash_area_to_nffs_desc(FLASH_AREA_IMAGE_0, &cnt, descs);
    img_starts[0] = 0;
    total = cnt;

    rc = flash_area_to_nffs_desc(FLASH_AREA_IMAGE_1, &cnt, &descs[total]);
    assert(rc == 0);
    img_starts[1] = total;
    total += cnt;

    rc = flash_area_to_nffs_desc(FLASH_AREA_IMAGE_SCRATCH, &cnt, &descs[total]);
    assert(rc == 0);
    req.br_scratch_area_idx = total;

    total += 1;
    req.br_num_image_areas = total;

    for (cnt = 0; cnt < total; cnt++) {
        img_areas[cnt] = cnt;
    }
    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    boot_jump(rsp.br_hdr, rsp.br_image_addr);

    return 0;
}

