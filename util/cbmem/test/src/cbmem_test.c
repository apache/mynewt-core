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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "testutil/testutil.h"
#include "cbmem/cbmem.h"
#include "cbmem_test.h"

struct cbmem cbmem1;
uint8_t cbmem1_buf[CBMEM1_BUF_SIZE];
uint8_t cbmem1_entry[CBMEM1_ENTRY_SIZE];

/*
 * Things to test.
 *
 * - Wrap of the circular buffer.
 * - Reading through all entries.
 */

void
setup_cbmem1(void *arg)
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

int
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

TEST_CASE_DECL(cbmem_test_case_1)
TEST_CASE_DECL(cbmem_test_case_2)
TEST_CASE_DECL(cbmem_test_case_3)

TEST_SUITE(cbmem_test_suite)
{
    cbmem_test_case_1();
    cbmem_test_case_2();
    cbmem_test_case_3();
}

#if MYNEWT_VAL(SELFTEST)

int
main(int argc, char **argv)
{
    sysinit();

    tu_suite_set_init_cb(setup_cbmem1, NULL);
    cbmem_test_suite();

    return tu_any_failed;
}

#endif
