/**
 * Copyright (c) 2015 Stack Inc.
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
#include "ffs/ffs.h"
#include "testutil/testutil.h"
#include "testutil_priv.h"

struct tu_config tu_config;
int tu_any_failed;
int tu_first_idx=0;

int
tu_init(void)
{
    int rc;

    if (tu_config.tc_base_path != NULL) {
        if (tu_config.tc_area_descs != NULL) {
            rc = flash_init();
            if (rc != 0) {
                return -1;
            }

            rc = ffs_init();
            if (rc != 0) {
                return -1;
            }

            rc = ffs_detect(tu_config.tc_area_descs);
            if (rc == FFS_ECORRUPT) {
                rc = ffs_format(tu_config.tc_area_descs);
            }
            if (rc != 0) {
                return -1;
            }
        }

        rc = tu_report_read_status();
        if (rc != 0) {
            tu_report_rmdir_results();
        }

        rc = tu_report_mkdir_results();
        if (rc != 0) {
            return -1;
        }

        rc = tu_report_mkdir_meta();
        if (rc != 0) {
            return -1;
        }
    }

    tu_any_failed = 0;

    return 0;
}

void
tu_restart(void)
{
    tu_case_write_pass_auto();

    tu_first_idx = tu_case_idx + 1;
    tu_report_write_status();

    tu_arch_restart();
}
