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

TEST_CASE_SELF(fcb_test_rotate)
{
    struct fcb *fcb;
    int rc;
    int old_id;
    struct fcb_entry loc;
    uint8_t test_data[128];
    int elem_cnts[2] = {0, 0};
    int cnts[2];
    struct append_arg aa_arg = {
        .elem_cnts = cnts
    };

    fcb_tc_pretest(2);

    fcb = &test_fcb;

    old_id = fcb->f_active_id;
    rc = fcb_rotate(fcb);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(fcb->f_active_id == old_id + 1);

    /*
     * Now fill up the
     */
    while (1) {
        rc = fcb_append(fcb, sizeof(test_data), &loc);
        if (rc == FCB_ERR_NOSPACE) {
            break;
        }
        if (loc.fe_area == &test_fcb_area[0]) {
            elem_cnts[0]++;
        } else if (loc.fe_area == &test_fcb_area[1]) {
            elem_cnts[1]++;
        } else {
            TEST_ASSERT(0);
        }

        rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data,
          sizeof(test_data));
        TEST_ASSERT(rc == 0);

        rc = fcb_append_finish(fcb, &loc);
        TEST_ASSERT(rc == 0);
    }
    TEST_ASSERT(elem_cnts[0] > 0 && elem_cnts[0] == elem_cnts[1]);

    old_id = fcb->f_active_id;
    rc = fcb_rotate(fcb);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(fcb->f_active_id == old_id); /* no new area created */

    memset(cnts, 0, sizeof(cnts));
    rc = fcb_walk(fcb, NULL, fcb_test_cnt_elems_cb, &aa_arg);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(aa_arg.elem_cnts[0] == elem_cnts[0] ||
      aa_arg.elem_cnts[1] == elem_cnts[1]);
    TEST_ASSERT(aa_arg.elem_cnts[0] == 0 || aa_arg.elem_cnts[1] == 0);

    /*
     * One sector is full. The other one should have one entry in it.
     */
    rc = fcb_append(fcb, sizeof(test_data), &loc);
    TEST_ASSERT(rc == 0);

    rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data,
      sizeof(test_data));
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(fcb, &loc);
    TEST_ASSERT(rc == 0);

    old_id = fcb->f_active_id;
    rc = fcb_rotate(fcb);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(fcb->f_active_id == old_id);

    memset(cnts, 0, sizeof(cnts));
    rc = fcb_walk(fcb, NULL, fcb_test_cnt_elems_cb, &aa_arg);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(aa_arg.elem_cnts[0] == 1 || aa_arg.elem_cnts[1] == 1);
    TEST_ASSERT(aa_arg.elem_cnts[0] == 0 || aa_arg.elem_cnts[1] == 0);
}
