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
#include "conf_test_fcb.h"

TEST_CASE_SELF(config_test_compress_reset)
{
    int rc;
    struct conf_fcb cf;
    struct flash_area *fa;
    char test_value[CONF_TEST_FCB_VAL_STR_CNT][CONF_MAX_VAL_LEN];
    int elems[4];
    int i;

    config_wipe_srcs();
    config_wipe_fcb(fcb_areas, sizeof(fcb_areas) / sizeof(fcb_areas[0]));

    cf.cf_fcb.f_magic = MYNEWT_VAL(CONFIG_FCB_MAGIC);
    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb_dst(&cf);
    TEST_ASSERT(rc == 0);

    c2_var_count = 1;
    memset(elems, 0, sizeof(elems));

    for (i = 0; ; i++) {
        config_test_fill_area(test_value, i);
        memcpy(val_string, test_value, sizeof(val_string));

        rc = conf_save();
        TEST_ASSERT(rc == 0);

        if (cf.cf_fcb.f_active.fe_area == &fcb_areas[2]) {
            /*
             * Started using space just before scratch.
             */
            break;
        }
        memset(val_string, 0, sizeof(val_string));

        rc = conf_load();
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(!memcmp(val_string, test_value, CONF_MAX_VAL_LEN));
    }

    fa = cf.cf_fcb.f_active.fe_area;
    rc = fcb_append_to_scratch(&cf.cf_fcb);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(fcb_free_sector_cnt(&cf.cf_fcb) == 0);
    TEST_ASSERT(fa != cf.cf_fcb.f_active.fe_area);

    config_wipe_srcs();

    memset(&cf, 0, sizeof(cf));

    cf.cf_fcb.f_magic = MYNEWT_VAL(CONFIG_FCB_MAGIC);
    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb_dst(&cf);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(fcb_free_sector_cnt(&cf.cf_fcb) == 1);
    TEST_ASSERT(fa == cf.cf_fcb.f_active.fe_area);

    c2_var_count = 0;
}
