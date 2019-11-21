/*
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

#include "flash_map_test.h"

static struct flash_area scratch_flash_map[100];

static int
flash_area_cmp(const struct flash_area *fa, const struct flash_area *fb)
{
    int64_t diff;

    diff = fb->fa_id - fa->fa_id;
    if (diff != 0) {
        return diff;
    }

    diff = fb->fa_device_id - fa->fa_device_id;
    if (diff != 0) {
        return diff;
    }

    diff = fb->fa_off - fa->fa_off;
    if (diff != 0) {
        return diff;
    }

    diff = fb->fa_size - fa->fa_size;
    if (diff != 0) {
        return diff;
    }

    return 0;
}

static int
flash_map_cmp(const struct flash_area *fma, int fma_size,
              const struct flash_area *fmb, int fmb_size)
{
    const struct flash_area *fa;
    const struct flash_area *fb;
    bool found;
    int diff;
    int ai;
    int bi;

    diff = fmb_size - fma_size;
    if (diff != 0) {
        return diff;
    }

    for (ai = 0; ai < fma_size; ai++) {
        fa = &fma[ai];

        found = false;
        for (bi = 0; bi < fmb_size; bi++) {
            fb = &fmb[bi];

            if (fa->fa_id == fb->fa_id) {
                diff = flash_area_cmp(fa, fb);
                if (diff != 0) {
                    return diff;
                }

                found = true;
                break;
            }
        }

        if (!found) {
            return 1;
        }
    }

    return 0;
}

TEST_CASE_SELF(flash_map_test_case_new_areas)
{
    const int sysflash_map_dflt_sz = 
        sizeof sysflash_map_dflt / sizeof sysflash_map_dflt[0];

    bool found;
    int scratch_num_areas;
    int rc;
    int i;

    /*** No new areas. */

    flash_map = scratch_flash_map;
    memcpy(scratch_flash_map, sysflash_map_dflt, sizeof sysflash_map_dflt);
    flash_map_entries = sysflash_map_dflt_sz;

    flash_map_add_new_dflt_areas_extern();
    rc = flash_map_cmp(flash_map, flash_map_entries,
                       sysflash_map_dflt, sysflash_map_dflt_sz);
    TEST_ASSERT(rc == 0);

    /*** All new areas. */

    flash_map = scratch_flash_map;
    flash_map_entries = 0;

    flash_map_add_new_dflt_areas_extern();
    rc = flash_map_cmp(flash_map, flash_map_entries,
                       sysflash_map_dflt, sysflash_map_dflt_sz);
    TEST_ASSERT(rc == 0);

    /*** One new area sandwiched between existing areas. */

    memcpy(scratch_flash_map, sysflash_map_dflt, sizeof sysflash_map_dflt);

    /* Remove image slot 1 from flash map. */
    found = false;
    for (i = 0; i < sysflash_map_dflt_sz - 1; i++) {
        if (!found && scratch_flash_map[i].fa_id == FLASH_AREA_IMAGE_1) {
            found = true;
        }

        if (found) {
            scratch_flash_map[i] = scratch_flash_map[i + 1];
        }
    }

    flash_map = scratch_flash_map;
    flash_map_entries = sysflash_map_dflt_sz - 1;

    flash_map_add_new_dflt_areas_extern();
    rc = flash_map_cmp(flash_map, flash_map_entries,
                       sysflash_map_dflt, sysflash_map_dflt_sz);
    TEST_ASSERT(rc == 0);

    /*** One new area that overlaps; ensure new area doesn't get added. */

    memcpy(scratch_flash_map, sysflash_map_dflt, sizeof sysflash_map_dflt);
    scratch_num_areas = sysflash_map_dflt_sz - 1;
    scratch_flash_map[scratch_num_areas - 1].fa_size += 4096;
    flash_map = scratch_flash_map;
    flash_map_entries = scratch_num_areas;

    flash_map_add_new_dflt_areas_extern();
    rc = flash_map_cmp(flash_map, flash_map_entries,
                       scratch_flash_map, scratch_num_areas);
    TEST_ASSERT(rc == 0);

    /***
     * Two new areas - one overlaps, the other doesn't.  Ensure only the
     * non-overlapping area gets added.
     */

    memcpy(scratch_flash_map, sysflash_map_dflt, sizeof sysflash_map_dflt);
    scratch_num_areas = sysflash_map_dflt_sz - 2;
    scratch_flash_map[0].fa_size += 4096;
    flash_map = scratch_flash_map;
    flash_map_entries = scratch_num_areas;

    flash_map_add_new_dflt_areas_extern();

    /* Ensure none of the common areas changed. */
    rc = flash_map_cmp(flash_map, scratch_num_areas,
                       scratch_flash_map, scratch_num_areas);
    TEST_ASSERT(rc == 0);

    /* Make sure only one area got added. */
    TEST_ASSERT_FATAL(flash_map_entries == scratch_num_areas + 1);

    /* Verify the correct area got added. */
    rc = flash_area_cmp(&flash_map[flash_map_entries - 1],
                        &sysflash_map_dflt[sysflash_map_dflt_sz - 1]);
    TEST_ASSERT_FATAL(rc == 0);
}
