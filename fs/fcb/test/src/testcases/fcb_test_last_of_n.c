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

TEST_CASE_SELF(fcb_test_last_of_n)
{
    const uint8_t ENTRIES = 5;
    struct fcb *fcb;
    int rc;
    struct fcb_entry loc;
    struct fcb_entry areas[ENTRIES];
    uint8_t test_data[128];
    uint8_t i;

    fcb_tc_pretest(4);

    fcb = &test_fcb;
    fcb->f_scratch_cnt = 1;

    /* No fcbs available */
    rc = fcb_offset_last_n(fcb, 1, &loc);
    assert (rc != 0);

    /*
     * Add some fcbs.
     */
    for (i = 0; i < ENTRIES; i++) {
        rc = fcb_append(fcb, sizeof(test_data), &loc);
        if (rc == FCB_ERR_NOSPACE) {
            break;
        }

        rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data,
          sizeof(test_data));
        TEST_ASSERT(rc == 0);

        rc = fcb_append_finish(fcb, &loc);
        TEST_ASSERT(rc == 0);

        areas[i] = loc;
    }

    /* last entry */
    rc = fcb_offset_last_n(fcb, 1, &loc);
    assert (rc == 0);
    assert (areas[4].fe_area == loc.fe_area);
    assert (areas[4].fe_data_off == loc.fe_data_off);
    assert (areas[4].fe_data_len == loc.fe_data_len);

    /* somewhere in the middle */
    rc = fcb_offset_last_n(fcb, 3, &loc);
    assert (rc == 0);
    assert (areas[2].fe_area == loc.fe_area);
    assert (areas[2].fe_data_off == loc.fe_data_off);
    assert (areas[2].fe_data_len == loc.fe_data_len);

    /* first entry */
    rc = fcb_offset_last_n(fcb, 5, &loc);
    assert (rc == 0);
    assert (areas[0].fe_area == loc.fe_area);
    assert (areas[0].fe_data_off == loc.fe_data_off);
    assert (areas[0].fe_data_len == loc.fe_data_len);

    /* after last valid entry, returns the first one like for 5 */
    rc = fcb_offset_last_n(fcb, 6, &loc);
    assert (rc == 0);
    assert (areas[0].fe_area == loc.fe_area);
    assert (areas[0].fe_data_off == loc.fe_data_off);
    assert (areas[0].fe_data_len == loc.fe_data_len);
}
