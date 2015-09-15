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

#include <stdio.h>
#include "testutil/testutil.h"
#include "testutil_priv.h"

#define TU_REPORT_META_DIR          ".meta"
#define TU_REPORT_STATUS_FILENAME   "status"

static char tu_report_buf[1024];

int
tu_report_rmdir_results(void)
{
    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    return tu_io_rmdir(tu_config.tc_base_path);
}

int
tu_report_mkdir_results(void)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s", tu_config.tc_base_path);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }
    return tu_io_mkdir(tu_report_buf);
}

int
tu_report_mkdir_meta(void)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/" TU_REPORT_META_DIR, tu_config.tc_base_path);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }
    return tu_io_mkdir(tu_report_buf);
}

int
tu_report_mkdir_suite(void)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/%s", tu_config.tc_base_path,
                  tu_suite_name);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }

    rc = tu_io_mkdir(tu_report_buf);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tu_report_mkdir_case(void)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/%s/%s", tu_config.tc_base_path,
                  tu_suite_name, tu_case_name);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }

    rc = tu_io_mkdir(tu_report_buf);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tu_report_write_file(const char *filename, const uint8_t *data,
                     size_t data_len)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/%s/%s/%s", tu_config.tc_base_path,
                  tu_suite_name, tu_case_name, filename);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }

    rc = tu_io_write(tu_report_buf, data, data_len);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tu_report_read_status(void)
{
    size_t bytes_read;
    int rc;

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/%s/%s", tu_config.tc_base_path,
                  TU_REPORT_META_DIR, TU_REPORT_STATUS_FILENAME);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }

    rc = tu_io_read(tu_report_buf, &tu_first_idx, sizeof tu_first_idx,
                    &bytes_read);
    if (rc != 0 || bytes_read != sizeof tu_first_idx) {
        return -1;
    }

    tu_io_delete(tu_report_buf);

    return 0;
}

int
tu_report_write_status(void)
{
    int rc;

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/%s/%s", tu_config.tc_base_path,
                  TU_REPORT_META_DIR, TU_REPORT_STATUS_FILENAME);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }

    rc = tu_io_write(tu_report_buf, &tu_first_idx, sizeof tu_first_idx);
    if (rc != 0) {
        return -1;
    }

    return 0;

}
