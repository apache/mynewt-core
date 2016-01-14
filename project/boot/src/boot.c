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
#include <hal/flash_map.h>
#include <os/os.h>
#include <hal/hal_system.h>
#include "nffs/nffs.h"
#include "bootutil/image.h"
#include "bootutil/loader.h"

#define NFFS_AREA_MAX	34
#define SEC_CNT_MAX	8

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

    cnt = (NFFS_AREA_MAX / 2) - 3;
    rc = flash_area_to_nffs_desc(FLASH_AREA_IMAGE_0, &cnt, descs);
    img_starts[0] = 0;
    total = cnt;

    cnt = (NFFS_AREA_MAX / 2) - 3;
    rc = flash_area_to_nffs_desc(FLASH_AREA_IMAGE_1, &cnt, &descs[total]);
    assert(rc == 0);
    img_starts[1] = total;
    total += cnt;

    cnt = 1;
    rc = flash_area_to_nffs_desc(FLASH_AREA_IMAGE_SCRATCH, &cnt, &descs[total]);
    assert(rc == 0);
    req.br_scratch_area_idx = total;
    total += 1;

    req.br_num_image_areas = total;

    for (cnt = 0; cnt < total; cnt++) {
        img_areas[cnt] = cnt;
    }

    cnt = 6;
    rc = flash_area_to_nffs_desc(FLASH_AREA_NFFS, &cnt, &descs[total]);
    assert(rc == 0);
    total += cnt;

    nffs_config.nc_num_inodes = 50;
    nffs_config.nc_num_blocks = 50;
    nffs_config.nc_num_cache_blocks = 32;

    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    system_start((void *)(rsp.br_image_addr + rsp.br_hdr->ih_hdr_size));

    return 0;
}

