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

TEST_CASE_SELF(fcb_test_append_too_big)
{
    struct fcb *fcb;
    int rc;
    int len;
    struct fcb_entry elem_loc;

    fcb_tc_pretest(2);

    fcb = &test_fcb;

    /*
     * Max element which fits inside sector is
     * sector size - (disk header + crc + 6 bytes of entry).
     */
    len = fcb->f_active.fe_range->fsr_sector_size;

    rc = fcb_append(fcb, len, &elem_loc);
    TEST_ASSERT(rc != 0);

    len--;
    rc = fcb_append(fcb, len, &elem_loc);
    TEST_ASSERT(rc != 0);

    len -= sizeof(struct fcb_disk_area);
    rc = fcb_append(fcb, len, &elem_loc);
    TEST_ASSERT(rc != 0);

    len = fcb->f_active.fe_range->fsr_sector_size -
      (sizeof(struct fcb_disk_area) + 2 + 6);
    rc = fcb_append(fcb, len, &elem_loc);
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(&elem_loc);
    TEST_ASSERT(rc == 0);

    rc = fcb_elem_info(&elem_loc);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(elem_loc.fe_data_len == len);
}
