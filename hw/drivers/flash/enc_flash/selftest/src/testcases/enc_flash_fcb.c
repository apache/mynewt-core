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

#include <fcb/fcb.h>
#include "enc_flash_test.h"

static void
enc_flash_test_fcb_init(struct fcb *fcb)
{
    fcb->f_magic = 0xdeadbeef;
    fcb->f_sector_cnt = ENC_TEST_FLASH_AREA_CNT;
    fcb->f_scratch_cnt = 0;
    fcb->f_sectors = enc_test_flash_areas;
}

TEST_CASE_SELF(enc_flash_test_fcb)
{
    int rc;
    struct flash_area *fa;
    struct fcb_entry loc;
    struct fcb fcb;
    int i;
    char *writedata = "foobartest";
    char readdata[128];

    fa = enc_test_flash_areas;
    for (i = 0; i < ENC_TEST_FLASH_AREA_CNT; i++) {
        rc = flash_area_erase(fa, 0, fa->fa_size);
        TEST_ASSERT(rc == 0);
        fa++;
    }

    enc_flash_test_fcb_init(&fcb);
    rc = fcb_init(&fcb);
    TEST_ASSERT(rc == 0);

    rc = fcb_append(&fcb, strlen(writedata), &loc);
    TEST_ASSERT(rc == 0);

    rc = flash_area_write(loc.fe_area, loc.fe_data_off, writedata,
                          strlen(writedata));
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(&fcb, &loc);
    TEST_ASSERT(rc == 0);

    memset(&loc, 0, sizeof(loc));
    rc = fcb_getnext(&fcb, &loc);
    TEST_ASSERT(rc == 0);

    memset(readdata, 0, sizeof(readdata));
    rc = flash_area_read(loc.fe_area, loc.fe_data_off, readdata,
                         loc.fe_data_len);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(!memcmp(readdata, writedata, strlen(writedata)));

    enc_flash_test_fcb_init(&fcb);
    rc = fcb_init(&fcb);
    TEST_ASSERT(rc == 0);

    memset(readdata, 0, sizeof(readdata));
    rc = flash_area_read(loc.fe_area, loc.fe_data_off, readdata,
                         loc.fe_data_len);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(!memcmp(readdata, writedata, strlen(writedata)));
}
