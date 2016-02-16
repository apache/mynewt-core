/**
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
#include <string.h>

#include <os/os.h>
#include <testutil/testutil.h>
#include <config/config.h>

static uint8_t val8;

static int test_get_called;
static int test_set_called;
static int test_commit_called;

static char *
ctest_handle_get(int argc, char **argv, char *val, int val_len_max)
{
    test_get_called = 1;
    if (argc == 1 && !strcmp(argv[0], "mybar")) {
        return conf_str_from_value(CONF_INT8, &val8, val, val_len_max);
    }
    return NULL;
}

static int
ctest_handle_set(int argc, char **argv, char *val)
{
    uint8_t newval;
    int rc;

    test_set_called = 1;
    if (argc == 1 && !strcmp(argv[0], "mybar")) {
        rc = CONF_VALUE_SET(val, CONF_INT8, newval);
        TEST_ASSERT(rc == 0);
        val8 = newval;
        return 0;
    }
    return OS_ENOENT;
}

static int
ctest_handle_commit(void)
{
    test_commit_called = 1;
    return 0;
}

struct conf_handler config_test_handler = {
    .ch_name = "myfoo",
    .ch_get = ctest_handle_get,
    .ch_set = ctest_handle_set,
    .ch_commit = ctest_handle_commit
};

static void
ctest_clear_call_state(void)
{
    test_get_called = 0;
    test_set_called = 0;
    test_commit_called = 0;
}

static int
ctest_get_call_state(void)
{
    return test_get_called + test_set_called + test_commit_called;
}

TEST_CASE(config_empty_lookups)
{
    int rc;
    char tmp[64], *str;

    rc = conf_set_value("foo/bar", "tmp");
    TEST_ASSERT(rc != 0);

    str = conf_get_value("foo/bar", tmp, sizeof(tmp));
    TEST_ASSERT(str == NULL);
}

TEST_CASE(config_test_insert)
{
    int rc;

    rc = conf_register(&config_test_handler);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(config_test_getset_unknown)
{
    char tmp[64], *str;
    int rc;

    rc = conf_set_value("foo/bar", "tmp");
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(ctest_get_call_state() == 0);

    str = conf_get_value("foo/bar", tmp, sizeof(tmp));
    TEST_ASSERT(str == NULL);
    TEST_ASSERT(ctest_get_call_state() == 0);

    rc = conf_set_value("myfoo/bar", "tmp");
    TEST_ASSERT(rc == OS_ENOENT);
    TEST_ASSERT(test_set_called == 1);
    ctest_clear_call_state();

    str = conf_get_value("myfoo/bar", tmp, sizeof(tmp));
    TEST_ASSERT(str == NULL);
    TEST_ASSERT(test_get_called == 1);
    ctest_clear_call_state();
}

TEST_CASE(config_test_getset_int)
{
    char tmp[64], *str;
    int rc;

    rc = conf_set_value("myfoo/mybar", "42");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_set_called == 1);
    TEST_ASSERT(val8 == 42);
    ctest_clear_call_state();

    str = conf_get_value("myfoo/mybar", tmp, sizeof(tmp));
    TEST_ASSERT(str);
    TEST_ASSERT(test_get_called == 1);
    TEST_ASSERT(!strcmp("42", tmp));
    ctest_clear_call_state();
}

TEST_CASE(config_test_commit)
{
    int rc;

    rc = conf_commit("bar");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(ctest_get_call_state());

    rc = conf_commit(NULL);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_commit_called == 1);
    ctest_clear_call_state();

    rc = conf_commit("myfoo");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_commit_called == 1);
    ctest_clear_call_state();
}

TEST_SUITE(config_test_suite)
{
    config_empty_lookups();
    config_test_insert();
    config_test_getset_unknown();
    config_test_getset_int();
    config_test_commit();
}

#ifdef PKG_TEST

int
main(int argc, char **argv)
{
    tu_config.tc_print_results = 1;
    tu_init();

    conf_init();
    config_test_suite();

    return tu_any_failed;
}

#endif
