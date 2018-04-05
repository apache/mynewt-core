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

#include <assert.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "testutil_priv.h"

const char *tu_suite_name = 0;
int tu_suite_failed = 0;

struct ts_testsuite_list g_ts_suites;

/*
 * tu_suite_register must be called for each test_suite that's to
 * be run from a list rather than explicitly called.
 * See mynewtsanity.
 */
int
tu_suite_register(tu_testsuite_fn_t* ts, const char *name)
{
    struct ts_suite *tsp;

    tsp = (struct ts_suite *)os_malloc(sizeof(*tsp));
    if (!tsp) {
        return -1;
    }
    tsp->ts_name = name;
    tsp->ts_test = ts;
    SLIST_INSERT_HEAD(&g_ts_suites, tsp, ts_next);
    return 0;
}

static void
tu_suite_set_name(const char *name)
{
    ts_current_config->ts_suite_name = name;
}

void
tu_suite_set_init_cb(tu_init_test_fn_t *cb, void *cb_arg)
{
    ts_config.ts_suite_init_cb = cb;
    ts_config.ts_suite_init_arg = cb_arg;
}

void
tu_suite_set_complete_cb(tu_init_test_fn_t *cb, void *cb_arg)
{
    ts_config.ts_suite_complete_cb = cb;
    ts_config.ts_suite_complete_arg = cb_arg;
}

/**
 * Configures a callback that gets executed at the end of each test
 * case in the current suite.  This is useful when there are some
 * checks that should be performed at the end of each test
 * (e.g., verify no memory leaks).  This callback is cleared when the
 * current suite completes.
 *
 * @param cb -  The callback to execute at the end of each test case.
 * @param cb_arg - An optional argument that gets passed to the
 *                                  callback.
 */
void
tu_suite_set_pre_test_cb(tu_pre_test_fn_t *cb, void *cb_arg)
{
    ts_config.ts_case_pre_test_cb = cb;
    ts_config.ts_case_pre_arg = cb_arg;
}

void
tu_suite_pre_test(void)
{
    if (ts_config.ts_case_pre_test_cb != NULL) {
        ts_config.ts_case_pre_test_cb(ts_config.ts_case_pre_arg);
    }
}

void
tu_suite_set_post_test_cb(tu_post_test_fn_t *cb, void *cb_arg)
{
    ts_config.ts_case_post_test_cb = cb;
    ts_config.ts_case_post_arg = cb_arg;
}

void
tu_suite_post_test(void)
{
    if (ts_config.ts_case_post_test_cb != NULL) {
        ts_config.ts_case_post_test_cb(ts_config.ts_case_post_arg);
    }
}

void
tu_suite_set_pass_cb(tu_case_report_fn_t *cb, void *cb_arg)
{
    ts_config.ts_case_pass_cb = cb;
    ts_config.ts_case_pass_arg = cb_arg;
}

void
tu_suite_set_fail_cb(tu_case_report_fn_t *cb, void *cb_arg)
{
    ts_config.ts_case_fail_cb = cb;
    ts_config.ts_case_fail_arg = cb_arg;
}

void
tu_suite_complete(void)
{
    tu_suite_set_init_cb(NULL, NULL);
    tu_suite_set_pre_test_cb(NULL, NULL);
    tu_suite_set_post_test_cb(NULL, NULL);
    if (ts_config.ts_suite_complete_cb != NULL) {
        ts_config.ts_suite_complete_cb(ts_config.ts_suite_complete_arg);
    }
    tu_suite_set_complete_cb(NULL, NULL);
}

void
tu_suite_init(const char *name)
{
    tu_suite_failed = 0;

    tu_suite_set_name(name);

    if (ts_config.ts_suite_init_cb != NULL) {
        ts_config.ts_suite_init_cb(ts_config.ts_suite_init_arg);
    }
}
