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
#include "conf_test_nffs.h"

TEST_CASE_SELF(config_test_empty_file)
{
    int rc;
    struct conf_file cf_mfg;
    struct conf_file cf_running;
    const char cf_mfg_test[] = "";
    const char cf_running_test[] = "\n\n";

    cf_mfg.cf_name = "/config/mfg";
    cf_running.cf_name = "/config/running";

    rc = conf_file_src(&cf_mfg);
    TEST_ASSERT(rc == 0);
    rc = conf_file_src(&cf_running);

    /*
     * No files
     */
    conf_load();

    rc = fs_mkdir("/config");
    TEST_ASSERT(rc == 0);

    rc = fsutil_write_file("/config/mfg", cf_mfg_test, sizeof(cf_mfg_test));
    TEST_ASSERT(rc == 0);

    rc = fsutil_write_file("/config/running", cf_running_test,
      sizeof(cf_running_test));
    TEST_ASSERT(rc == 0);

    conf_load();
    ctest_clear_call_state();
}
