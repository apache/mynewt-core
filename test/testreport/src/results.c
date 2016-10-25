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

#include <stdio.h>
#include "testutil/testutil.h"
#include "testreport/testreport.h"
#include "testreport_priv.h"

#define TU_REPORT_META_DIR          ".meta"
#define TU_REPORT_STATUS_FILENAME   "status"

static char tr_report_buf[1024];

int
tr_report_rmdir_results(void)
{
    if (tr_config.tc_base_path == NULL) {
        return 0;
    }

    return tr_io_rmdir(tr_config.tc_base_path);
}

int
tr_report_mkdir_results(void)
{
    int rc;

    if (tr_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tr_report_buf, sizeof tr_report_buf,
                  "%s", tr_config.tc_base_path);
    if (rc >= sizeof tr_report_buf) {
        return -1;
    }
    return tr_io_mkdir(tr_report_buf);
}

int
tr_report_mkdir_meta(void)
{
    int rc;

    if (tr_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tr_report_buf, sizeof tr_report_buf,
                  "%s/" TU_REPORT_META_DIR, tr_config.tc_base_path);
    if (rc >= sizeof tr_report_buf) {
        return -1;
    }
    return tr_io_mkdir(tr_report_buf);
}

int
tr_report_mkdir_suite(void)
{
    int rc;

    if (tr_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tr_report_buf, sizeof tr_report_buf,
                  "%s/%s", tr_config.tc_base_path,
                  tu_suite_name);
    if (rc >= sizeof tr_report_buf) {
        return -1;
    }

    rc = tr_io_mkdir(tr_report_buf);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tr_report_mkdir_case(void)
{
    int rc;

    if (tr_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tr_report_buf, sizeof tr_report_buf,
                  "%s/%s/%s", tr_config.tc_base_path,
                  tu_suite_name, tu_case_name);
    if (rc >= sizeof tr_report_buf) {
        return -1;
    }

    rc = tr_io_mkdir(tr_report_buf);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tr_report_write_file(const char *filename, const uint8_t *data,
                     size_t data_len)
{
    int rc;

    if (tr_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tr_report_buf, sizeof tr_report_buf,
                  "%s/%s/%s/%s", tr_config.tc_base_path,
                  tu_suite_name, tu_case_name, filename);
    if (rc >= sizeof tr_report_buf) {
        return -1;
    }

    rc = tr_io_write(tr_report_buf, data, data_len);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tr_report_read_status(void)
{
    size_t bytes_read;
    int rc;

    rc = snprintf(tr_report_buf, sizeof tr_report_buf,
                  "%s/%s/%s", tr_config.tc_base_path,
                  TU_REPORT_META_DIR, TU_REPORT_STATUS_FILENAME);
    if (rc >= sizeof tr_report_buf) {
        return -1;
    }

    rc = tr_io_read(tr_report_buf, &tu_first_idx, sizeof tu_first_idx,
                    &bytes_read);
    if (rc != 0 || bytes_read != sizeof tu_first_idx) {
        return -1;
    }

    tr_io_delete(tr_report_buf);

    return 0;
}

int
tr_report_write_status(void)
{
    int rc;

    rc = snprintf(tr_report_buf, sizeof tr_report_buf,
                  "%s/%s/%s", tr_config.tc_base_path,
                  TU_REPORT_META_DIR, TU_REPORT_STATUS_FILENAME);
    if (rc >= sizeof tr_report_buf) {
        return -1;
    }

    rc = tr_io_write(tr_report_buf, &tu_first_idx, sizeof tu_first_idx);
    if (rc != 0) {
        return -1;
    }

    return 0;

}
