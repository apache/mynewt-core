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

TEST_CASE_SELF(config_test_save_2_fcb)
{
    int rc;
    struct conf_fcb cf;
    char test_value[CONF_TEST_FCB_VAL_STR_CNT][CONF_MAX_VAL_LEN];
    int i;

    config_wipe_srcs();

    cf.cf_fcb.f_magic = MYNEWT_VAL(CONFIG_FCB_MAGIC);
    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb_dst(&cf);
    TEST_ASSERT(rc == 0);

    config_test_fill_area(test_value, 0);
    memcpy(val_string, test_value, sizeof(val_string));

    val8 = 42;
    rc = conf_save();
    TEST_ASSERT(rc == 0);

    val8 = 0;
    memset(val_string[0], 0, sizeof(val_string[0]));
    rc = conf_load();
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(val8 == 42);
    TEST_ASSERT(!strcmp(val_string[0], test_value[0]));
    test_export_block = 1;

    /*
     * Now add the number of settings to max. Keep adjusting the test_data,
     * check that rollover happens when it's supposed to.
     */
    c2_var_count = 64;

    for (i = 0; i < 32; i++) {
        config_test_fill_area(test_value, i);
        memcpy(val_string, test_value, sizeof(val_string));

        rc = conf_save();
        TEST_ASSERT(rc == 0);

        memset(val_string, 0, sizeof(val_string));

        val8 = 0;
        rc = conf_load();
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(!memcmp(val_string, test_value, sizeof(val_string)));
        TEST_ASSERT(val8 == 42);
    }
    c2_var_count = 0;
}
