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

/*
 * Entries that do not fit in buffer are rejected
 */
TEST_CASE_SELF(cbmem_test_case_too_long)
{
    struct cbmem_scat_gath_entry entries[2];
    struct cbmem_scat_gath sg;
    struct cbmem cbmem;
    struct {
        uint8_t buf[64];
        uint8_t guard[16];
    } mem;
    uint8_t data[64];
    int rc;

    memset(&mem, 0xa5, sizeof(mem));
    memset(data, 0x11, sizeof(data));
    cbmem_init(&cbmem, mem.buf, sizeof(mem.buf));

    rc = cbmem_append(&cbmem, data, sizeof(mem.buf));
    TEST_ASSERT(rc != 0);

    rc = cbmem_append(&cbmem, data, sizeof(mem.buf) - sizeof(struct cbmem_entry_hdr));
    TEST_ASSERT(rc == 0);

    /* Total length does not fit in 16 bits */
    entries[0].om = NULL;
    entries[0].flat_buf = data;
    entries[0].flat_len = 0x8000;
    entries[1] = entries[0];
    sg.entries = entries;
    sg.count = 2;
    rc = cbmem_append_scat_gath(&cbmem, &sg);
    TEST_ASSERT(rc != 0);

    TEST_ASSERT(mem.guard[0] == 0xa5);
}
