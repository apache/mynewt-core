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
#include "hal/hal_flash.h"
#include "testutil/testutil.h"
#include "testutil_priv.h"

struct tu_config tu_config;
int tu_any_failed;
int tu_first_idx;

int
tu_init(void)
{
    tu_any_failed = 0;

    return 0;
}

void
tu_restart(void)
{
    tu_case_write_pass_auto();

    tu_first_idx = tu_case_idx + 1;

    if (tu_config.tc_restart_cb != NULL) {
        tu_config.tc_restart_cb(tu_config.tc_restart_arg);
    }

    tu_arch_restart();
}
