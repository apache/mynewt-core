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
#include "fcb_test.h"

TEST_CASE_SELF(fcb_test_reset)
{
    struct fcb *fcb;
    int rc;
    int i;
    struct fcb_entry loc;
    uint8_t test_data[128];
    int var_cnt;

    fcb_tc_pretest(2);

    fcb = &test_fcb;

    var_cnt = 0;
    rc = fcb_walk(fcb, FCB_SECTOR_OLDEST, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 0);

    rc = fcb_append(fcb, 32, &loc);
    TEST_ASSERT(rc == 0);

    /*
     * No ready ones yet. CRC should not match.
     */
    var_cnt = 0;
    rc = fcb_walk(fcb, FCB_SECTOR_OLDEST, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 0);

    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = fcb_test_append_data(32, i);
    }
    rc = fcb_write(&loc, 0, test_data, 32);
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(&loc);
    TEST_ASSERT(rc == 0);

    /*
     * one entry
     */
    var_cnt = 32;
    rc = fcb_walk(fcb, FCB_SECTOR_OLDEST, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 33);

    /*
     * Pretend reset
     */
    memset(fcb, 0, sizeof(*fcb));
    fcb->f_range_cnt = 1;
    fcb->f_sector_cnt = 2;
    fcb->f_ranges = test_fcb_ranges;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    var_cnt = 32;
    rc = fcb_walk(fcb, FCB_SECTOR_OLDEST, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 33);

    rc = fcb_append(fcb, 33, &loc);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = fcb_test_append_data(33, i);
    }
    rc = fcb_write(&loc, 0, test_data, 33);
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(&loc);
    TEST_ASSERT(rc == 0);

    var_cnt = 32;
    rc = fcb_walk(fcb, FCB_SECTOR_OLDEST, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 34);

    /*
     * Add partial one, make sure that we survive reset then.
     */
    rc = fcb_append(fcb, 34, &loc);
    TEST_ASSERT(rc == 0);

    memset(fcb, 0, sizeof(*fcb));
    fcb->f_range_cnt = 1;
    fcb->f_sector_cnt = 2;
    fcb->f_ranges = test_fcb_ranges;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    /*
     * Walk should skip that.
     */
    var_cnt = 32;
    rc = fcb_walk(fcb, FCB_SECTOR_OLDEST, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 34);

    /* Add a 3rd one, should go behind corrupt entry */
    rc = fcb_append(fcb, 34, &loc);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = fcb_test_append_data(34, i);
    }
    rc = fcb_write(&loc, 0, test_data, 34);
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(&loc);
    TEST_ASSERT(rc == 0);

    /*
     * Walk should skip corrupt entry, but report the next one.
     */
    var_cnt = 32;
    rc = fcb_walk(fcb, FCB_SECTOR_OLDEST, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 35);
}
