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

TEST_CASE_SELF(config_test_multiple_in_file)
{
    int rc;
    struct conf_file cf_mfg;
    const char cf_mfg_test1[] =
      "myfoo/mybar=1\n"
      "myfoo/mybar=14";
    const char cf_mfg_test2[] =
      "myfoo/mybar=1\n"
      "myfoo/mybar=15\n"
      "\n";

    cf_mfg.cf_name = "/config/mfg";
    rc = conf_file_src(&cf_mfg);
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/config");
    TEST_ASSERT_FATAL(rc == 0);

    rc = fsutil_write_file("/config/mfg", cf_mfg_test1, sizeof(cf_mfg_test1));
    TEST_ASSERT(rc == 0);

    conf_load();
    TEST_ASSERT(test_set_called);
    TEST_ASSERT(val8 == 14);

    rc = fsutil_write_file("/config/mfg", cf_mfg_test2, sizeof(cf_mfg_test2));
    TEST_ASSERT(rc == 0);

    conf_load();
    TEST_ASSERT(test_set_called);
    TEST_ASSERT(val8 == 15);
}
