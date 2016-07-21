/**
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
#include "testutil/testutil.h"
#include "testutil_priv.h"

const char *tu_suite_name = 0;
int tu_suite_failed = 0;

static void
tu_suite_set_name(const char *name)
{
    tu_suite_name = name;
}

/**
 * Configures a callback that gets executed at the end of each test case in the
 * current suite.  This is useful when there are some checks that should be
 * performed at the end of each test (e.g., verify no memory leaks).  This
 * callback is cleared when the current suite completes.
 *
 * @param cb                    The callback to execute at the end of each test
 *                                  case.
 * @param cb_arg                An optional argument that gets passed to the
 *                                  callback.
 */
void
tu_suite_set_post_test_cb(tu_post_test_fn_t *cb, void *cb_arg)
{
    tu_case_post_test_cb = cb;
    tu_case_post_test_cb_arg = cb_arg;
}

void
tu_suite_complete(void)
{
    tu_suite_set_post_test_cb(NULL, NULL);
}

void
tu_suite_init(const char *name)
{
    tu_suite_failed = 0;

    tu_suite_set_name(name);

    if (tu_config.tc_suite_init_cb != NULL) {
        tu_config.tc_suite_init_cb(tu_config.tc_suite_init_arg);
    }
}
