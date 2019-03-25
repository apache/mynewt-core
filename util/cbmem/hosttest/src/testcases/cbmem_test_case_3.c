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

TEST_CASE(cbmem_test_case_3)
{
    struct cbmem_entry_hdr *hdr;
    struct cbmem_iter iter;
    uint16_t off;
    uint16_t len;
    uint8_t buf[128];
    int i;
    int rc;

    i = 0;
    cbmem_iter_start(&cbmem1, &iter);
    while (1) {
        hdr = cbmem_iter_next(&cbmem1, &iter);
        if (hdr == NULL) {
            break;
        }

        /* first ensure we can read the entire entry */
        off = 0;
        len = 0;
        while (1) {
            rc = cbmem_read(&cbmem1, hdr, buf, off, sizeof(buf));
            TEST_ASSERT_FATAL(rc >= 0,
                    "Error reading from buffer rc=%d, off=%d,len=%d", rc, off,
                    sizeof(buf));
            if (rc == 0) {
                break;
            }
            off += rc;
            len += rc;
        }
        TEST_ASSERT_FATAL(len == CBMEM1_ENTRY_SIZE,
                "Couldn't read full entry, expected %d got %d",
                CBMEM1_ENTRY_SIZE, len);
        i++;

        /* go apesh*t, and read data out of bounds, see what we get. */
        rc = cbmem_read(&cbmem1, hdr, buf, CBMEM1_ENTRY_SIZE * 2, sizeof(buf));
        TEST_ASSERT_FATAL(rc < 0,
                "Reading invalid should return error, instead %d returned.",
                rc);
    }
}
