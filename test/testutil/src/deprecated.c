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

#include "testutil/testutil.h"

void
tu_suite_set_init_cb(tu_init_test_fn_t *cb, void *cb_arg)
{
    tu_config.deprecated.ts_suite_init_cb = cb;
    tu_config.deprecated.ts_suite_init_arg = cb_arg;
}

void
tu_suite_set_complete_cb(tu_init_test_fn_t *cb, void *cb_arg)
{
    tu_config.deprecated.ts_suite_complete_cb = cb;
    tu_config.deprecated.ts_suite_complete_arg = cb_arg;
}

void
tu_suite_set_post_test_cb(tu_post_test_fn_t *cb, void *cb_arg)
{
    tu_config.deprecated.tc_case_post_test_cb = cb;
    tu_config.deprecated.tc_case_post_arg = cb_arg;
}

void
tu_suite_set_pass_cb(tu_case_report_fn_t *cb, void *cb_arg)
{
    tu_set_pass_cb(cb, cb_arg);
}

void
tu_suite_set_fail_cb(tu_case_report_fn_t *cb, void *cb_arg)
{
    tu_set_fail_cb(cb, cb_arg);
}

void
tu_case_set_init_cb(tu_init_test_fn_t *cb, void *cb_arg)
{
    tu_config.deprecated.tc_case_init_cb = cb;
    tu_config.deprecated.tc_case_init_arg = cb_arg;
}

void
tu_case_set_pre_cb(tu_pre_test_fn_t *cb, void *cb_arg)
{
    tu_config.deprecated.tc_case_pre_test_cb = cb;
    tu_config.deprecated.tc_case_pre_arg = cb_arg;
}

void
tu_case_set_post_cb(tu_post_test_fn_t *cb, void *cb_arg)
{
    tu_case_set_post_test_cb(cb, cb_arg);
}
