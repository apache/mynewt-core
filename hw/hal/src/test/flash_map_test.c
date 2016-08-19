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
#include <stdio.h>
#include <string.h>

#include <os/os.h>
#include <testutil/testutil.h>
#include "hal/flash_map.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_flash_int.h"

/*
 * Test flash_area_to_sectors()
 */
TEST_CASE(flash_map_test_case_1)
{
    const struct flash_area *fa;
    int areas_checked = 0;
    int i, j, rc;
    const struct hal_flash *hf;
    struct flash_area my_secs[32];
    int my_sec_cnt;
    uint32_t end;

    os_init();

    for (i = 0; i < 8; i++) {
        rc = flash_area_open(i, &fa);
        if (rc) {
            continue;
        }

        hf = bsp_flash_dev(fa->fa_flash_id);
        TEST_ASSERT_FATAL(hf != NULL, "bsp_flash_dev");

        rc = flash_area_to_sectors(i, &my_sec_cnt, my_secs);
        TEST_ASSERT_FATAL(rc == 0, "flash_area_to_sectors failed");

        end = fa->fa_off;
        for (j = 0; j < my_sec_cnt; j++) {
            TEST_ASSERT_FATAL(end == my_secs[j].fa_off, "Non contiguous area");
            TEST_ASSERT_FATAL(my_secs[j].fa_flash_id == fa->fa_flash_id,
              "Sectors not in same flash?");
            end = my_secs[j].fa_off + my_secs[j].fa_size;
        }
        if (my_sec_cnt) {
            areas_checked++;
            TEST_ASSERT_FATAL(my_secs[my_sec_cnt - 1].fa_off +
              my_secs[my_sec_cnt - 1].fa_size == fa->fa_off + fa->fa_size,
              "Last sector not in the end");
        }
    }
    TEST_ASSERT_FATAL(areas_checked != 0, "No flash map areas to check!");
}

/*
 * Test flash_erase
 */
TEST_CASE(flash_map_test_case_2)
{
    const struct flash_area *fa;
    struct flash_area secs[32];
    int sec_cnt;
    int i;
    int rc;
    uint32_t off;
    uint8_t wd[256];
    uint8_t rd[256];

    os_init();

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fa);
    TEST_ASSERT_FATAL(rc == 0, "flash_area_open() fail");

    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_0, &sec_cnt, secs);
    TEST_ASSERT_FATAL(rc == 0, "flash_area_to_sectors failed");

    /*
     * First erase the area so it's ready for use.
     */
    for (i = 0; i < sec_cnt; i++) {
        rc = hal_flash_erase_sector(secs[i].fa_flash_id, secs[i].fa_off);
        TEST_ASSERT_FATAL(rc == 0, "hal_flash_erase_sector() failed");
    }
    TEST_ASSERT_FATAL(rc == 0, "read data != write data");

    memset(wd, 0xa5, sizeof(wd));

    /* write stuff to beginning of every sector */
    off = 0;
    for (i = 0; i < sec_cnt; i++) {
        rc = flash_area_write(fa, off, wd, sizeof(wd));
        TEST_ASSERT_FATAL(rc == 0, "flash_area_write() fail");

        /* read it back via hal_flash_Read() */
        rc = hal_flash_read(fa->fa_flash_id, fa->fa_off + off, rd, sizeof(rd));
        TEST_ASSERT_FATAL(rc == 0, "hal_flash_read() fail");

        rc = memcmp(wd, rd, sizeof(wd));
        TEST_ASSERT_FATAL(rc == 0, "read data != write data");

        /* write stuff to end of area */
        rc = hal_flash_write(fa->fa_flash_id,
          fa->fa_off + off + secs[i].fa_size - sizeof(wd), wd, sizeof(wd));
        TEST_ASSERT_FATAL(rc == 0, "hal_flash_write() fail");

        /* and read it back */
        memset(rd, 0, sizeof(rd));
        rc = flash_area_read(fa, off + secs[i].fa_size - sizeof(rd),
          rd, sizeof(rd));
        TEST_ASSERT_FATAL(rc == 0, "hal_flash_read() fail");

        rc = memcmp(wd, rd, sizeof(rd));
        TEST_ASSERT_FATAL(rc == 0, "read data != write data");

        off += secs[i].fa_size;
    }
    /* erase it */
    rc = flash_area_erase(fa, 0, fa->fa_size);
    TEST_ASSERT_FATAL(rc == 0, "read data != write data");

    /* should read back ff all throughout*/
    memset(wd, 0xff, sizeof(wd));
    for (off = 0; off < fa->fa_size; off += sizeof(rd)) {
         rc = flash_area_read(fa, off, rd, sizeof(rd));
         TEST_ASSERT_FATAL(rc == 0, "hal_flash_read() fail");

         rc = memcmp(wd, rd, sizeof(rd));
         TEST_ASSERT_FATAL(rc == 0, "area not erased");
    }
}

TEST_SUITE(flash_map_test_suite)
{
    flash_map_test_case_1();
    flash_map_test_case_2();
}

#ifdef MYNEWT_SELFTEST

int
main(int argc, char **argv)
{
    tu_config.tc_print_results = 1;
    tu_init();

    flash_map_test_suite();

    return tu_any_failed;
}

#endif
