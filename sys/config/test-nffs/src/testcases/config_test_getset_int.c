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

TEST_CASE_SELF(config_test_getset_int)
{
    char name[80];
    char tmp[64], *str;
    int rc;

    strcpy(name, "myfoo/mybar");
    rc = conf_set_value(name, "42");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_set_called == 1);
    TEST_ASSERT(val8 == 42);
    ctest_clear_call_state();

    strcpy(name, "myfoo/mybar");
    str = conf_get_value(name, tmp, sizeof(tmp));
    TEST_ASSERT(str);
    TEST_ASSERT(test_get_called == 1);
    TEST_ASSERT(!strcmp("42", tmp));
    ctest_clear_call_state();
}
