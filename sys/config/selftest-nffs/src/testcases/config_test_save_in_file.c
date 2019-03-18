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

TEST_CASE_SELF(config_test_save_in_file)
{
    int rc;
    struct conf_file cf;

    rc = fs_mkdir("/config");
    TEST_ASSERT(rc == 0 || rc == FS_EEXIST);

    cf.cf_name = "/config/blah";
    rc = conf_file_src(&cf);
    TEST_ASSERT(rc == 0);
    rc = conf_file_dst(&cf);
    TEST_ASSERT(rc == 0);

    val8 = 8;
    rc = conf_save();
    TEST_ASSERT(rc == 0);

    rc = conf_test_file_strstr(cf.cf_name, "myfoo/mybar=8\n");
    TEST_ASSERT(rc == 0);

    val8 = 43;
    rc = conf_save();
    TEST_ASSERT(rc == 0);

    rc = conf_test_file_strstr(cf.cf_name, "myfoo/mybar=43\n");
    TEST_ASSERT(rc == 0);
}
