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
#include <stdio.h>
#include "hal/hal_flash.h"
#include "nffs/nffs.h"
#include "testutil/testutil.h"
#include "testreport/testreport.h"
#include "testreport_priv.h"

struct tr_config tr_config;

static int tr_case_fail_idx;

static void
tr_case_init(void *unused)
{
    int rc;

    rc = tr_results_mkdir_case();
    assert(rc == 0);
}

static void
tr_case_fail(char *msg, int msg_len, void *unused)
{
    char filename[14];
    int rc;

    rc = snprintf(filename, sizeof filename, "fail-%04d.txt",
                  tr_case_fail_idx);
    assert(rc < sizeof filename);

    rc = tr_results_write_file(filename, (uint8_t *)msg, msg_len);
    assert(rc == 0);

    tr_case_fail_idx++;
}

static void
tr_case_pass(char *msg, int msg_len, void *unused)
{
    int rc;

    rc = tr_results_write_file("pass.txt", (uint8_t *)msg, msg_len);
    assert(rc == 0);
}

static void
tr_suite_init(void *unused)
{
    int rc;

    rc = tr_results_mkdir_suite();
    assert(rc == 0);
}

static void
tr_restart(void *unused)
{
    tr_results_write_status();
}

int
tr_init(void)
{
    int rc;

    if (tr_config.tc_base_path != NULL) {
        if (tr_config.tc_area_descs != NULL) {
            rc = flash_init();
            if (rc != 0) {
                return -1;
            }

            rc = nffs_init();
            if (rc != 0) {
                return -1;
            }

            rc = nffs_detect(tr_config.tc_area_descs);
            if (rc == NFFS_ECORRUPT) {
                rc = nffs_format(tr_config.tc_area_descs);
            }
            if (rc != 0) {
                return -1;
            }
        }

        rc = tr_results_read_status();
        if (rc != 0) {
            tr_results_rmdir_results();
        }

        rc = tr_results_mkdir_results();
        if (rc != 0) {
            return -1;
        }

        rc = tr_results_mkdir_meta();
        if (rc != 0) {
            return -1;
        }
    }

    tu_config.tc_case_init_cb = tr_case_init;
    tu_config.tc_case_fail_cb = tr_case_fail;
    tu_config.tc_case_pass_cb = tr_case_pass;
    tu_config.tc_suite_init_cb = tr_suite_init;
    tu_config.tc_restart_cb = tr_restart;

    return 0;
}
