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

TEST_CASE_SELF(config_test_save_one_file)
{
    int rc;
    struct conf_file cf;

    rc = fs_mkdir("/config");
    TEST_ASSERT(rc == 0);

    cf.cf_name = "/config/blah";
    rc = conf_file_src(&cf);
    TEST_ASSERT(rc == 0);
    rc = conf_file_dst(&cf);
    TEST_ASSERT(rc == 0);

    val8 = 33;
    rc = conf_save();
    TEST_ASSERT(rc == 0);

    rc = conf_save_one("myfoo/mybar", "42");
    TEST_ASSERT(rc == 0);

    rc = conf_load();
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(val8 == 42);

    rc = conf_save_one("myfoo/mybar", "44");
    TEST_ASSERT(rc == 0);

    rc = conf_load();
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(val8 == 44);
}
