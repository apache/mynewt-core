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

TEST_CASE_SELF(config_test_compress_reset)
{
    int rc;
    struct conf_fcb2 cf;
    uint16_t fa;
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

    fa = cf.cf2_fcb.f_active_id;
    rc = fcb2_append_to_scratch(&cf.cf2_fcb);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(fcb2_free_sector_cnt(&cf.cf2_fcb) == 0);
    TEST_ASSERT(fa != cf.cf2_fcb.f_active_id);

    config_wipe_srcs();

    memset(&cf, 0, sizeof(cf));

    cf.cf2_fcb.f_magic = MYNEWT_VAL(CONFIG_FCB_MAGIC);
    cf.cf2_fcb.f_range_cnt = CONF_TEST_FCB_RANGE_CNT;
    cf.cf2_fcb.f_sector_cnt = fcb_range[0].fsr_sector_count;
    cf.cf2_fcb.f_ranges = fcb_range;

    rc = conf_fcb2_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb2_dst(&cf);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(fcb2_free_sector_cnt(&cf.cf2_fcb) == 1);
    TEST_ASSERT(fa == cf.cf2_fcb.f_active_id);

    c2_var_count = 0;
}
