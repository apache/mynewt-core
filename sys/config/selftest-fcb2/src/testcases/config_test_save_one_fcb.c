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

TEST_CASE_SELF(config_test_save_one_fcb)
{
    int rc;
    struct conf_fcb2 cf;

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

    val8 = 33;
    rc = conf_save();
    TEST_ASSERT(rc == 0);

    rc = conf_save_one("myfoo/mybar", "42");
    TEST_ASSERT(rc == 0);

    rc = conf_load();
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(val8 == 42);

    rc = conf_save_one("myfoo/mybar", "44");
    TEST_ASSERT(rc == 0);

    rc = conf_load();
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(val8 == 44);
}
