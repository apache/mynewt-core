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

TEST_CASE_SELF(log_test_case_cbmem_printf)
{
    struct cbmem cbmem;
    struct log log;
    int i;
    uint16_t len = 0;
    int num_strs = ltu_num_strs();
    uint16_t *off_arr;
    struct log_entry_hdr *hdr;
    char data[256];

    ltu_setup_cbmem(&cbmem, &log);
    len = ltu_init_arr();
    TEST_ASSERT_FATAL(len != 0);

    off_arr = ltu_get_ltu_off_arr();
    TEST_ASSERT_FATAL(off_arr != NULL);

    for (i = 0; i < num_strs; i++) {
	hdr = (struct log_entry_hdr *)(dummy_log_arr + off_arr[i]);
        len = off_arr[i+1] - off_arr[i] - log_hdr_len(hdr) - log_trailer_len(&log, hdr);
	memcpy(data, dummy_log_arr + off_arr[i] + log_hdr_len(hdr),
	       len);
	data[len] = '\0';
        log_printf(&log, 0, 0, data, len);
    }

    ltu_verify_contents(&log);
}
