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
#include "conf_test_fcb.h"

/* Fill unused stack with non-zero pattern so that uninitialized stack
 * variables of the function called next do not happen to be zero.
 */
static void __attribute__((noinline))
config_test_dirty_stack(void)
{
    volatile uint8_t buf[1024];

    memset((void *)buf, 0xa5, sizeof(buf));
}

TEST_CASE_SELF(config_test_getset_unknown)
{
    static const char *const no_tokens[] = { "", "/", "///" };
    char name[80];
    char tmp[64], *str;
    int rc;
    int i;

    strcpy(name, "foo/bar");
    rc = conf_set_value(name, "tmp");
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(ctest_get_call_state() == 0);

    strcpy(name, "foo/bar");
    str = conf_get_value(name, tmp, sizeof(tmp));
    TEST_ASSERT(str == NULL);
    TEST_ASSERT(ctest_get_call_state() == 0);

    strcpy(name, "myfoo/bar");
    rc = conf_set_value(name, "tmp");
    TEST_ASSERT(rc == OS_ENOENT);
    TEST_ASSERT(test_set_called == 1);
    ctest_clear_call_state();

    strcpy(name, "myfoo/bar");
    str = conf_get_value(name, tmp, sizeof(tmp));
    TEST_ASSERT(str == NULL);
    TEST_ASSERT(test_get_called == 1);
    ctest_clear_call_state();

    /* Names without any tokens */
    for (i = 0; i < ARRAY_SIZE(no_tokens); i++) {
        strcpy(name, no_tokens[i]);
        config_test_dirty_stack();
        rc = conf_set_value(name, "tmp");
        TEST_ASSERT(rc != 0);
        TEST_ASSERT(ctest_get_call_state() == 0);

        strcpy(name, no_tokens[i]);
        config_test_dirty_stack();
        str = conf_get_value(name, tmp, sizeof(tmp));
        TEST_ASSERT(str == NULL);
        TEST_ASSERT(ctest_get_call_state() == 0);

        strcpy(name, no_tokens[i]);
        config_test_dirty_stack();
        rc = conf_commit(name);
        TEST_ASSERT(rc != 0);
        TEST_ASSERT(ctest_get_call_state() == 0);
    }
}
