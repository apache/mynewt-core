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

TEST_CASE_SELF(fcb_test_append_fill)
{
    struct fcb2 *fcb;
    int rc;
    int i;
    struct fcb2_entry loc;
    uint8_t test_data[128];
    int elem_cnts[2] = {0, 0};
    int aa_together_cnts[2];
    struct append_arg aa_together = {
        .elem_cnts = aa_together_cnts
    };
    int aa_separate_cnts[2];
    struct append_arg aa_separate = {
        .elem_cnts = aa_separate_cnts
    };

    fcb_tc_pretest(2);

    fcb = &test_fcb;

    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = fcb_test_append_data(sizeof(test_data), i);
    }

    while (1) {
        rc = fcb2_append(fcb, sizeof(test_data), &loc);
        if (rc == FCB2_ERR_NOSPACE) {
            break;
        }
        if (loc.fe_sector == 0) {
            elem_cnts[0]++;
        } else if (loc.fe_sector == 1) {
            elem_cnts[1]++;
        } else {
            TEST_ASSERT(0);
        }

        rc = fcb2_write(&loc, 0, test_data, sizeof(test_data));
        TEST_ASSERT(rc == 0);

        rc = fcb2_append_finish(&loc);
        TEST_ASSERT(rc == 0);
    }
    TEST_ASSERT(elem_cnts[0] > 0);
    TEST_ASSERT(elem_cnts[0] == elem_cnts[1]);

    memset(&aa_together_cnts, 0, sizeof(aa_together_cnts));
    rc = fcb2_walk(fcb, FCB2_SECTOR_OLDEST, fcb_test_cnt_elems_cb, &aa_together);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(aa_together.elem_cnts[0] == elem_cnts[0]);
    TEST_ASSERT(aa_together.elem_cnts[1] == elem_cnts[1]);

    memset(&aa_separate_cnts, 0, sizeof(aa_separate_cnts));
    rc = fcb2_walk(fcb, 0, fcb_test_cnt_elems_cb, &aa_separate);
    TEST_ASSERT(rc == 0);
    rc = fcb2_walk(fcb, 1, fcb_test_cnt_elems_cb, &aa_separate);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(aa_separate.elem_cnts[0] == elem_cnts[0]);
    TEST_ASSERT(aa_separate.elem_cnts[1] == elem_cnts[1]);

    /* Re-init FCB without clearing flash */
    rc = fcb_tc_init_fcb(2);
    TEST_ASSERT(rc == 0);
    aa_together_cnts[0] = 0;
    aa_together_cnts[1] = 0;
    /* Walk again, number of elements should not changed */
    rc = fcb2_walk(fcb, FCB2_SECTOR_OLDEST, fcb_test_cnt_elems_cb, &aa_together);
    TEST_ASSERT(aa_together_cnts[0] == elem_cnts[0]);
    TEST_ASSERT(aa_together_cnts[1] == elem_cnts[1]);
}
