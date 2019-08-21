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
#include "conf_test_fcb2.h"

static int unique_val_cnt;

static int
test_custom_compress_filter1(const char *val, const char *name, void *arg)
{
    unique_val_cnt++;
    return 0;
}

static int
test_custom_compress_filter2(const char *val, const char *name, void *arg)
{
    if (!strcmp(val, "myfoo/mybar")) {
        return 0;
    }
    return 1;
}

TEST_CASE_SELF(config_test_custom_compress)
{
    int rc;
    struct conf_fcb2 cf;
    char test_value[CONF_TEST_FCB_VAL_STR_CNT][CONF_MAX_VAL_LEN];
    int elems[4];
    int i;

    config_wipe_srcs();
    config_wipe_fcb2(fcb_range, CONF_TEST_FCB_RANGE_CNT);

    cf.cf2_fcb.f_magic = MYNEWT_VAL(CONFIG_FCB_MAGIC);
    cf.cf2_fcb.f_range_cnt = CONF_TEST_FCB_RANGE_CNT;
    cf.cf2_fcb.f_sector_cnt = fcb_range[0].fsr_sector_count;
    cf.cf2_fcb.f_ranges = fcb_range;

    rc = conf_fcb2_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb2_dst(&cf);
    TEST_ASSERT(rc == 0);

    c2_var_count = 1;
    test_export_block = 0;
    val8 = 4;
    val64 = 8;
    memset(elems, 0, sizeof(elems));

    for (i = 0; ; i++) {
        config_test_fill_area(test_value, i);
        memcpy(val_string, test_value, sizeof(val_string));

        rc = conf_save();
        TEST_ASSERT(rc == 0);

        if (cf.cf2_fcb.f_active_id == fcb_range[0].fsr_sector_count - 2) {
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

    for (i = 0; i < cf.cf2_fcb.f_sector_cnt - 1; i++) {
        conf_fcb2_compress(&cf, test_custom_compress_filter1, NULL);
    }
    TEST_ASSERT(unique_val_cnt == 4); /* c2, c3 and ctest together */

    test_export_block = 1;

    /*
     * Read values back, make sure they were carried over.
     */
    memset(val_string, 0, sizeof(val_string));
    val8 = 0;
    val64 = 0;
    rc = conf_load();
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(!memcmp(val_string, test_value, CONF_MAX_VAL_LEN));
    TEST_ASSERT(val8 == 4);
    TEST_ASSERT(val64 == 8);

    /*
     * Only leave one var.
     */
    for (i = 0; i < cf.cf2_fcb.f_sector_cnt - 1; i++) {
        conf_fcb2_compress(&cf, test_custom_compress_filter2, NULL);
    }

    memset(val_string, 0, sizeof(val_string));
    val8 = 0;
    val64 = 0;
    rc = conf_load();
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(val_string[0][0] == 0);
    TEST_ASSERT(val8 == 4);
    TEST_ASSERT(val64 == 0);
}
