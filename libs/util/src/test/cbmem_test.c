/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>

#include "testutil/testutil.h"
#include "util/cbmem.h" 

#define CBMEM1_BUF_SIZE (64 * 1024)

struct cbmem cbmem1;
uint8_t cbmem1_buf[CBMEM1_BUF_SIZE];
uint8_t cbmem1_entry[1024];

/*
 * Things to test.
 *
 * - Wrap of the circular buffer.  
 * - Reading through all entries.
 */

static void
setup_cbmem1(void)
{
    int i;
    int rc;

    rc = cbmem_init(&cbmem1, cbmem1_buf, CBMEM1_BUF_SIZE);
    TEST_ASSERT_FATAL(rc == 0, "cbmem_init() failed, non-zero RC = %d", rc);

    memset(cbmem1_entry, 0xff, sizeof(cbmem1_entry));

    /* Insert 65 1024 entries, and overflow buffer.  
     * This should overflow two entries, because the buffer is sized for 64 
     * entries, and then the headers themselves will eat into one of the entries, 
     * so there should be a total of 63 entries.
     * Ensure no data corruption.
     */
    for (i = 0; i < 65; i++) {
        cbmem1_entry[0] = i;
        rc = cbmem_append(&cbmem1, cbmem1_entry, sizeof(cbmem1_entry));
        TEST_ASSERT_FATAL(rc == 0, "Could not append entry %d, rc = %d", i, rc);
    }
}

static int 
cbmem_test_case_1_walk(struct cbmem *cbmem, struct cbmem_entry_hdr *hdr, 
        void *arg)
{
    uint8_t expected;
    uint8_t actual;
    int rc;

    expected = *(uint8_t *) arg;

    rc = cbmem_read(cbmem, hdr, &actual, 0, sizeof(actual));
    TEST_ASSERT_FATAL(rc == 1, "Couldn't read 1 byte from cbmem");
    TEST_ASSERT_FATAL(actual == expected, 
            "Actual doesn't equal expected (%d = %d)", actual, expected);

    *(uint8_t *) arg = ++expected;

    return (0);
}

TEST_CASE(cbmem_test_case_1) 
{
    int i;
    int rc;

    /* i starts at 2, for the 2 overwritten entries. */
    i = 2;
    rc = cbmem_walk(&cbmem1, cbmem_test_case_1_walk, &i);
    TEST_ASSERT_FATAL(rc == 0, "Could not walk cbmem tree!  rc = %d", rc);
    TEST_ASSERT_FATAL(i == 65, 
            "Did not go through every element of walk, %d processed", i - 2);

}

TEST_CASE(cbmem_test_case_2)
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
    TEST_ASSERT_FATAL(i == 65, 
            "Did not iterate through all 63 elements of CBMEM1, processed %d", 
            i - 2);
}

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
        TEST_ASSERT_FATAL(len == 1024, 
                "Couldn't read full entry, expected %d got %d", 1024, len);
        i++;

        /* go apesh*t, and read data out of bounds, see what we get. */
        rc = cbmem_read(&cbmem1, hdr, buf, 2048, sizeof(buf));
        TEST_ASSERT_FATAL(rc < 0, 
                "Reading invalid should return error, instead %d returned.",
                rc);
    }
}

TEST_SUITE(cbmem_test_suite)
{
    setup_cbmem1();
    cbmem_test_case_1();
    cbmem_test_case_2();
    cbmem_test_case_3();
}
