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

#include "cbmem_test/cbmem_test.h"

TEST_CASE_SELF(cbmem_test_case_2)
{
    struct cbmem_entry_hdr *hdr;
    struct cbmem_iter iter;
    uint8_t i;
    uint8_t val;
    int rc;

    i = 2;
    cbmem_iter_start(&cbmem1, &iter);
    while (1) {
        hdr = cbmem_iter_next(&cbmem1, &iter);
        if (hdr == NULL) {
            break;
        }

        rc = cbmem_read(&cbmem1, hdr, &val, 0, sizeof(val));
        TEST_ASSERT_FATAL(rc == 1, "Couldn't read 1 byte from cbmem");
        TEST_ASSERT_FATAL(val == i, "Entry index does not match %d vs %d",
                val, i);

        i++;
    }

    /* i starts at 2, for the 2 overwritten entries */
    TEST_ASSERT_FATAL(i == CBMEM1_ENTRY_COUNT + 1,
            "Did not iterate through all %d elements of CBMEM1, processed %d",
            CBMEM1_ENTRY_COUNT - 1, i - 2);
}
