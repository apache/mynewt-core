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

TEST_CASE_SELF(fcb_test_area_info)
{
    struct fcb *fcb;
    int rc;
    int i;
    struct fcb_entry loc;
    uint8_t test_data[128];
    int elem_cnts[2] = {0, 0};
    int area_elems[2];
    int area_bytes[2];

    fcb_tc_pretest(2);

    fcb = &test_fcb;

    /* check that sector outside of range fails */
    rc = fcb_area_info(fcb, fcb->f_sector_cnt, area_elems, area_bytes);
    TEST_ASSERT(rc == FCB_ERR_ARGS);
    rc = fcb_area_info(fcb, fcb->f_sector_cnt + 1, area_elems, area_bytes);
    TEST_ASSERT(rc == FCB_ERR_ARGS);

    /* output arguments are optional */
    rc = fcb_area_info(fcb, 0, NULL, NULL);
    TEST_ASSERT(rc == 0);

    /*
     * Should be empty, with elem count and byte count zero.
     */
    for (i = 0; i < 2; i++) {
        rc = fcb_area_info(fcb, 0, &area_elems[i],
                           &area_bytes[i]);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(area_elems[i] == 0);
        TEST_ASSERT(area_bytes[i] == 0);
    }

    /*
     * Fill up the areas, make sure that reporting is ok.
     */
    while (1) {
        rc = fcb_append(fcb, sizeof(test_data), &loc);
        if (rc == FCB_ERR_NOSPACE) {
            break;
        }
        if (loc.fe_sector == 0) {
            elem_cnts[0]++;
        } else if (loc.fe_sector == 1) {
            elem_cnts[1]++;
        } else {
            TEST_ASSERT(0);
        }

        rc = fcb_write(&loc, 0, test_data, sizeof(test_data));
        TEST_ASSERT(rc == 0);

        rc = fcb_append_finish(&loc);
        TEST_ASSERT(rc == 0);

        for (i = 0; i < 2; i++) {
            rc = fcb_area_info(fcb, i, &area_elems[i],
                               &area_bytes[i]);
            TEST_ASSERT(rc == 0);
            TEST_ASSERT(area_elems[i] == elem_cnts[i]);
            TEST_ASSERT(area_bytes[i] == elem_cnts[i] * sizeof(test_data));
        }
    }

    /*
     * Wipe out the oldest, should report zeroes for that area now.
     */
    rc = fcb_rotate(fcb);
    TEST_ASSERT(rc == 0);

    rc = fcb_area_info(fcb, 0, &area_elems[0], &area_bytes[0]);
    TEST_ASSERT(rc == 0);
    rc = fcb_area_info(fcb, 1, &area_elems[1], &area_bytes[1]);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(area_elems[0] == 0);
    TEST_ASSERT(area_bytes[0] == 0);
    TEST_ASSERT(area_elems[1] == elem_cnts[1]);
    TEST_ASSERT(area_bytes[1] == elem_cnts[1] * sizeof(test_data));

    /*
     * Test oldest sector should be sector area 1.
     */
    rc = fcb_area_info(fcb, FCB_SECTOR_OLDEST, &area_elems[0], &area_bytes[0]);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(area_elems[0] == elem_cnts[1]);
    TEST_ASSERT(area_bytes[0] == elem_cnts[1] * sizeof(test_data));
}
