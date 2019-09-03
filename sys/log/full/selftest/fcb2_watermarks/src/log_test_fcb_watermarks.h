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

#ifndef H_LOG_TEST_FCB2_WATERMARKS_
#define H_LOG_TEST_FCB2_WATERMARKS_

#include "os/mynewt.h"
#include "testutil/testutil.h"

struct ltfwu_cfg {
    int skip_mod;
    int body_len;
    int pop_count;
};

void ltfwu_test_once(const struct ltfwu_cfg *cfg);

TEST_CASE_DECL(log_test_case_fcb_watermarks_s0);
TEST_CASE_DECL(log_test_case_fcb_watermarks_s10);
TEST_CASE_DECL(log_test_case_fcb_watermarks_s100);

#endif
