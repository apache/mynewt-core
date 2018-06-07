#ifndef H_MODLOG_TEST_
#define H_MODLOG_TEST_

#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "log/log.h"
#include "modlog/modlog.h"
#include "modlog_test_util.h"

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

TEST_SUITE_DECL(modlog_test_suite_all);
TEST_CASE_DECL(modlog_test_case_append);
TEST_CASE_DECL(modlog_test_case_basic);
TEST_CASE_DECL(modlog_test_case_printf);
TEST_CASE_DECL(modlog_test_case_prio);

#endif
