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
#include <nffs/nffs.h>
#include <fs/fs.h>
#include <fs/fsutil.h>
#include "config/config.h"
#include "config/config_file.h"
#include "test/config_test.h"

static uint8_t val8;

static int test_get_called;
static int test_set_called;
static int test_commit_called;

extern SLIST_HEAD(, conf_store) conf_load_srcs; /* config_store.c */

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
    char name[80];
    char tmp[64], *str;

    strcpy(name, "foo/bar");
    rc = conf_set_value(name, "tmp");
    TEST_ASSERT(rc != 0);

    strcpy(name, "foo/bar");
    str = conf_get_value(name, tmp, sizeof(tmp));
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
    char name[80];
    char tmp[64], *str;
    int rc;

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
}

TEST_CASE(config_test_getset_int)
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

TEST_CASE(config_test_commit)
{
    char name[80];
    int rc;

    strcpy(name, "bar");
    rc = conf_commit(name);
    TEST_ASSERT(rc);
    TEST_ASSERT(ctest_get_call_state() == 0);

    rc = conf_commit(NULL);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_commit_called == 1);
    ctest_clear_call_state();

    strcpy(name, "myfoo");
    rc = conf_commit(name);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_commit_called == 1);
    ctest_clear_call_state();
}

static const struct nffs_area_desc config_nffs[] = {
    { 0x00000000, 16 * 1024 },
    { 0x00004000, 16 * 1024 },
    { 0x00008000, 16 * 1024 },
    { 0x0000c000, 16 * 1024 },
};

static void config_setup_nffs(void)
{
    int rc;

    rc = nffs_init();
    TEST_ASSERT(rc == 0);
    rc = nffs_format(config_nffs);
    TEST_ASSERT(rc == 0);
}

static void config_wipe_srcs(void)
{
    SLIST_INIT(&conf_load_srcs);
}

TEST_CASE(config_test_empty_file)
{
    int rc;
    struct conf_file cf_mfg;
    struct conf_file cf_running;
    const char cf_mfg_test[] = "";
    const char cf_running_test[] = "\n\n";

    config_wipe_srcs();

    cf_mfg.cf_name = "/config/mfg";
    cf_running.cf_name = "/config/running";

    rc = conf_file_register(&cf_mfg);
    TEST_ASSERT(rc == 0);
    rc = conf_file_register(&cf_running);

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
    config_wipe_srcs();
    ctest_clear_call_state();
}

TEST_CASE(config_test_small_file)
{
    int rc;
    struct conf_file cf_mfg;
    struct conf_file cf_running;
    const char cf_mfg_test[] = "myfoo/mybar=1";
    const char cf_running_test[] = " myfoo/mybar = 8 ";

    config_wipe_srcs();

    cf_mfg.cf_name = "/config/mfg";
    cf_running.cf_name = "/config/running";

    rc = conf_file_register(&cf_mfg);
    TEST_ASSERT(rc == 0);
    rc = conf_file_register(&cf_running);

    rc = fsutil_write_file("/config/mfg", cf_mfg_test, sizeof(cf_mfg_test));
    TEST_ASSERT(rc == 0);

    conf_load();
    TEST_ASSERT(test_set_called);
    TEST_ASSERT(val8 == 1);

    ctest_clear_call_state();

    rc = fsutil_write_file("/config/running", cf_running_test,
      sizeof(cf_running_test));
    TEST_ASSERT(rc == 0);

    conf_load();
    TEST_ASSERT(test_set_called);
    TEST_ASSERT(val8 == 8);

    ctest_clear_call_state();
}

TEST_CASE(config_test_multiple_in_file)
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

    config_wipe_srcs();

    cf_mfg.cf_name = "/config/mfg";
    rc = conf_file_register(&cf_mfg);
    TEST_ASSERT(rc == 0);

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

TEST_SUITE(config_test_all)
{
    config_empty_lookups();
    config_test_insert();
    config_test_getset_unknown();
    config_test_getset_int();
    config_test_commit();

    config_setup_nffs();
    config_test_empty_file();
    config_test_small_file();
    config_test_multiple_in_file();
}

