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

TEST_CASE_SELF(log_test_case_cbmem_append)
{
    struct cbmem cbmem;
    struct log log;
    uint8_t buf[256];
    char *str;
    int body_len;
    int i;

    ltu_setup_cbmem(&cbmem, &log);

    for (i = 0; ; i++) {
        str = ltu_str_logs[i];
        if (!str) {
            break;
        }

        body_len = strlen(str);
        memcpy(buf + LOG_ENTRY_HDR_SIZE, str, body_len);
        log_append_typed(&log, 0, 0, LOG_ETYPE_STRING, buf, body_len);
    }

    ltu_verify_contents(&log);
}
