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

TEST_CASE_SELF(fcb_test_getprev)
{
    struct fcb *fcb = &test_fcb;
    struct fcb_entry loc;
    struct fcb_entry prev;
    int rc;
    int i, j;

    fcb_tc_pretest(3);

    prev.fe_range = NULL;

    /*
     * Empty FCB returns error.
     */
    rc = fcb_getprev(fcb, &prev);
    TEST_ASSERT_FATAL(rc == FCB_ERR_NOVAR);

    /*
     * Add one entry. getprev should find that guy, and then error.
     */
    rc = fcb_append(fcb, 8, &loc);
    TEST_ASSERT(rc == 0);
    rc = fcb_append_finish(&loc);
    TEST_ASSERT(rc == 0);

    prev.fe_range = NULL;
    rc = fcb_getprev(fcb, &prev);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(!memcmp(&loc, &prev, sizeof(loc)));

    rc = fcb_getprev(fcb, &prev);
    TEST_ASSERT(rc == FCB_ERR_NOVAR);

    /*
     * Add enough entries to go to 2 sectors, should find them all.
     */
    fcb_tc_pretest(3);
    for (i = 0; ; i++) {
        rc = fcb_append(fcb, i + 1, &loc);
        TEST_ASSERT(rc == 0);
        rc = fcb_append_finish(&loc);
        TEST_ASSERT(rc == 0);

        if (loc.fe_sector == prev.fe_sector + 2) {
            break;
        }
    }

    prev.fe_range = NULL;
    for (j = i; j >= 0; j--) {
        rc = fcb_getprev(fcb, &prev);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(prev.fe_data_len == j + 1);
    }
    rc = fcb_getprev(fcb, &prev);
    TEST_ASSERT(rc == FCB_ERR_NOVAR);

    /*
     * Clean the area. Fill 2 whole sectors with corrupt entries. And one
     * good one. Should find the one good one, followed by error
     */
    fcb_tc_pretest(3);

    for (i = 0; ; i++) {
        rc = fcb_append(fcb, i + 1, &loc);
        TEST_ASSERT(rc == 0);

        if (loc.fe_sector == prev.fe_sector + 2) {
            rc = fcb_append_finish(&loc);
            TEST_ASSERT(rc == 0);
            break;
        }
    }

    prev.fe_range = NULL;
    rc = fcb_getprev(fcb, &prev);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(!memcmp(&loc, &prev, sizeof(loc)));

    rc = fcb_getprev(fcb, &prev);
    TEST_ASSERT(rc == FCB_ERR_NOVAR);

    /*
     * Create new. Rotate one sector, should be able to follow
     * from end of range to start.
     */

    fcb_tc_pretest(3);

    for (i = 0; ; i++) {
        rc = fcb_append(fcb, i + 8, &loc);
        if (rc == FCB_ERR_NOSPACE) {
            break;
        }
        TEST_ASSERT(rc == 0);
        rc = fcb_append_finish(&loc);
        TEST_ASSERT(rc == 0);
    }

    /*
     * Full. Rotate, add one more, and then walk backwards.
     */
    fcb_rotate(fcb);

    rc = fcb_append(fcb, i + 8, &loc);
    TEST_ASSERT(rc == 0);
    rc = fcb_append_finish(&loc);
    TEST_ASSERT(rc == 0);

    for (j = i; j >= 0; j--) {
        rc = fcb_getprev(fcb, &prev);
        if (rc == FCB_ERR_NOVAR) {
            TEST_ASSERT(i > 0);
            break;
        }
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(prev.fe_data_len == j + 8);
    }
}
