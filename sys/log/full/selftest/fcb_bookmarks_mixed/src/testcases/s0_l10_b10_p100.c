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
#include "log_test_fcb_bookmarks.h"

TEST_CASE_SELF(log_test_case_fcb_bookmarks_s0_l10_b10_p100)
{
    struct ltfbu_cfg cfg = {
        .skip_mod = 0,
        .body_len = 10,
        .bmark_count = 10,
        .pop_count = 100,
    };
    ltfbu_test_once(&cfg);
}
