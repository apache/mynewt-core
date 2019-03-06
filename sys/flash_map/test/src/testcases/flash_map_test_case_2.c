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

extern struct flash_area *fa_sectors;

/*
 * Test flash_erase
 */
TEST_CASE_SELF(flash_map_test_case_2)
{
    const struct flash_area *fa;
    int sec_cnt;
    int i;
    int rc;
    uint32_t off;
    uint8_t wd[256];
    uint8_t rd[256];

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fa);
    TEST_ASSERT_FATAL(rc == 0, "flash_area_open() fail");

    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_0, &sec_cnt, fa_sectors);
    TEST_ASSERT_FATAL(rc == 0, "flash_area_to_sectors failed");

    /*
     * First erase the area so it's ready for use.
     */
    for (i = 0; i < sec_cnt; i++) {
        rc = hal_flash_erase_sector(fa_sectors[i].fa_device_id,
                                    fa_sectors[i].fa_off);
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
        rc = hal_flash_read(fa->fa_device_id, fa->fa_off + off, rd, sizeof(rd));
        TEST_ASSERT_FATAL(rc == 0, "hal_flash_read() fail");

        rc = memcmp(wd, rd, sizeof(wd));
        TEST_ASSERT_FATAL(rc == 0, "read data != write data");

        /* write stuff to end of area */
        rc = hal_flash_write(fa->fa_device_id,
                         fa->fa_off + off + fa_sectors[i].fa_size - sizeof(wd),
                         wd, sizeof(wd));
        TEST_ASSERT_FATAL(rc == 0, "hal_flash_write() fail");

        /* and read it back */
        memset(rd, 0, sizeof(rd));
        rc = flash_area_read(fa, off + fa_sectors[i].fa_size - sizeof(rd),
          rd, sizeof(rd));
        TEST_ASSERT_FATAL(rc == 0, "hal_flash_read() fail");

        rc = memcmp(wd, rd, sizeof(rd));
        TEST_ASSERT_FATAL(rc == 0, "read data != write data");

        off += fa_sectors[i].fa_size;
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
