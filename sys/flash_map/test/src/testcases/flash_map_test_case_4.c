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
#include <console/console.h>
#include "flash_map_test.h"

#define FLASH_TEST_MAX_SECTORS 64

extern struct flash_area *fa_sectors;

static struct flash_area *cmp_secs;

static int
flash_map_test_case_4_chk_areas(const struct flash_area *fap, int cnt,
                                const struct flash_area *src)
{
    int i;

    for (i = 1; i < cnt; i++) {
        /*
         * Returned array of flash areas should be contiguous.
         */
        if (fap[i].fa_off != fap[i - 1].fa_off + fap[i - 1].fa_size) {
            return -1;
        }
    }
    if (fap[0].fa_off != src->fa_off) {
        return -2;
    }
    if (fap[cnt - 1].fa_off + fap[cnt - 1].fa_size !=
        src->fa_off + src->fa_size) {
        return -3;
    }
    return 0;
}

/*
 * Test flash_area_to_sectors_max
 */
TEST_CASE(flash_map_test_case_4)
{
    int areas[] = {
        FLASH_AREA_BOOTLOADER,
        FLASH_AREA_IMAGE_0,
        FLASH_AREA_IMAGE_1,
        FLASH_AREA_IMAGE_SCRATCH,
        FLASH_AREA_REBOOT_LOG,
        FLASH_AREA_NFFS
    };
    const struct flash_area *fap;
    int i;
    int sec_cnt;
    int new_max;
    int aid;
    int j;
    int rc;

    cmp_secs = (struct flash_area *)malloc(sizeof(struct flash_area) *
                                           FLASH_TEST_MAX_SECTORS);
    TEST_ASSERT_FATAL(cmp_secs);

    for (i = 0; i < sizeof(areas) / sizeof(areas[0]); i++) {
        aid = areas[i];

        rc = flash_area_open(aid, &fap);
        TEST_ASSERT_FATAL(rc == 0);

        rc = flash_area_to_sectors(aid, &sec_cnt, NULL);
        TEST_ASSERT_FATAL(rc == 0, "flash_area_to_sectors failed");

        printf("%d - %d\n", aid, sec_cnt);
        for (j = 1; j < 8; j++) {
            new_max = sec_cnt / j;

            if (new_max < 1) {
                printf("Skipping this test\n");
                continue;
            }
            memset(cmp_secs, 0, sizeof(struct flash_area) * sec_cnt);

            rc = flash_area_to_subareas(aid, &new_max, cmp_secs);
            TEST_ASSERT_FATAL(rc == 0, "flash_area_to_subareas failed");
            TEST_ASSERT(new_max <= sec_cnt / j);

            printf(" %d/%d -> %d %x\n",
                   sec_cnt, j, new_max, cmp_secs[0].fa_size);

            rc = flash_map_test_case_4_chk_areas(cmp_secs, new_max, fap);
            TEST_ASSERT(rc == 0);
        }
        flash_area_close(fap);
    }
    free(cmp_secs);
}
