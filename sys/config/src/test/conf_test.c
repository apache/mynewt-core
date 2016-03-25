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
#include "config/../../src/config_priv.h"

static uint8_t val8;

static int test_get_called;
static int test_set_called;
static int test_commit_called;

static char *ctest_handle_get(int argc, char **argv, char *val,
  int val_len_max);
static int ctest_handle_set(int argc, char **argv, char *val);
static int ctest_handle_commit(void);
static int ctest_handle_export(void (*cb)(struct conf_handler *, char *name,
    char *value));

struct conf_handler config_test_handler = {
    .ch_name = "myfoo",
    .ch_get = ctest_handle_get,
    .ch_set = ctest_handle_set,
    .ch_commit = ctest_handle_commit,
    .ch_export = ctest_handle_export
};

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

static int
ctest_handle_export(void (*cb)(struct conf_handler *, char *name, char *value))
{
    char value[32];

    conf_str_from_value(CONF_INT8, &val8, value, sizeof(value));
    cb(&config_test_handler, "mybar", value);

    return 0;
}

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
    { 0, 0 }
};

TEST_CASE(config_setup_nffs)
{
    int rc;

    rc = nffs_init();
    TEST_ASSERT_FATAL(rc == 0);
    rc = nffs_format(config_nffs);
    TEST_ASSERT_FATAL(rc == 0);
}

static void config_wipe_srcs(void)
{
    SLIST_INIT(&conf_load_srcs);
    conf_save_dst = NULL;
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

    rc = conf_file_src(&cf_mfg);
    TEST_ASSERT(rc == 0);
    rc = conf_file_src(&cf_running);

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
    rc = conf_file_src(&cf_mfg);
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

int
conf_test_file_strstr(const char *fname, char *string)
{
    int rc;
    uint32_t len;
    uint32_t rlen;
    char *buf;
    struct fs_file *file;

    rc = fs_open(fname, FS_ACCESS_READ, &file);
    if (rc) {
        return rc;
    }
    rc = fs_filelen(file, &len);
    fs_close(file);
    if (rc) {
        return rc;
    }

    buf = (char *)malloc(len + 1);
    TEST_ASSERT(buf);

    rc = fsutil_read_file(fname, 0, len, buf, &rlen);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(rlen == len);
    buf[rlen] = '\0';

    if (strstr(buf, string)) {
        return 0;
    } else {
        return -1;
    }
}

TEST_CASE(config_test_save_in_file)
{
    int rc;
    struct conf_file cf;

    config_wipe_srcs();

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

    config_test_save_in_file();
}

