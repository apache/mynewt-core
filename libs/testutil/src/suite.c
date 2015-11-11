/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

void
tu_suite_init(const char *name)
{
    tu_suite_failed = 0;

    tu_suite_set_name(name);

    if (tu_config.tc_suite_init_cb != NULL) {
        tu_config.tc_suite_init_cb(tu_config.tc_suite_init_arg);
    }
}
