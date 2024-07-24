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

#include "log_test_util/log_test_util.h"

TEST_CASE_SELF(log_test_case_fcb_append_mbuf)
{
    struct fcb_log fcb_log;
    struct os_mbuf *om;
    struct log log;
    int rc;
    int i;
    int num_strs = ltu_num_strs();
    struct log_entry_hdr *hdr;
    uint16_t *off_arr;
    uint16_t len;

    ltu_setup_fcb(&fcb_log, &log);
    len = ltu_init_arr();
    TEST_ASSERT_FATAL(len != 0);

    off_arr = ltu_get_ltu_off_arr();
    TEST_ASSERT_FATAL(off_arr != NULL);

    for (i = 0; i < num_strs; i++) {
        hdr = (struct log_entry_hdr *)(dummy_log_arr + off_arr[i]);
        len = off_arr[i+1] - off_arr[i] - log_trailer_len(&log, hdr);
        /* Split chain into several mbufs. */
        om = ltu_flat_to_fragged_mbuf(dummy_log_arr + off_arr[i],
                                      len, 2);

        rc = log_append_mbuf_typed(&log, 2, 3, LOG_ETYPE_STRING, om);
        TEST_ASSERT_FATAL(rc == 0);
    }

    ltu_verify_contents(&log);
}
