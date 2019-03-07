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
#include <string.h>

#include "os/mynewt.h"
#include <testutil/testutil.h>
#include <nffs/nffs.h>
#include <fs/fs.h>
#include <fs/fsutil.h>
#include "config/config.h"
#include "config/config_file.h"
#include "config_priv.h"
#include "conf_test_nffs.h"

uint8_t val8;
int c2_var_count = 1;
char val_string[64][CONF_MAX_VAL_LEN];

uint32_t val32;
uint64_t val64;

int test_get_called;
int test_set_called;
int test_commit_called;
int test_export_block;


static char *ctest_handle_get(int argc, char **argv, char *val,
                              int val_len_max);
static int ctest_handle_set(int argc, char **argv, char *val);
static int ctest_handle_commit(void);
static int ctest_handle_export(void (*cb)(char *name, char *value),
                               enum conf_export_tgt tgt);

struct conf_handler config_test_handler = {
    .ch_name = "myfoo",
    .ch_get = ctest_handle_get,
    .ch_set = ctest_handle_set,
    .ch_commit = ctest_handle_commit,
    .ch_export = ctest_handle_export
};

char *
ctest_handle_get(int argc, char **argv, char *val, int val_len_max)
{
    test_get_called = 1;
    if (argc == 1 && !strcmp(argv[0], "mybar")) {
        return conf_str_from_value(CONF_INT8, &val8, val, val_len_max);
    }
    if (argc == 1 && !strcmp(argv[0], "mybar64")) {
        return conf_str_from_value(CONF_INT64, &val64, val, val_len_max);
    }
    return NULL;
}

int
ctest_handle_set(int argc, char **argv, char *val)
{
    uint8_t newval;
    uint64_t newval64;
    int rc;

    test_set_called = 1;
    if (argc == 1 && !strcmp(argv[0], "mybar")) {
        rc = CONF_VALUE_SET(val, CONF_INT8, newval);
        TEST_ASSERT(rc == 0);
        val8 = newval;
        return 0;
    }
    if (argc == 1 && !strcmp(argv[0], "mybar64")) {
        rc = CONF_VALUE_SET(val, CONF_INT64, newval64);
        TEST_ASSERT(rc == 0);
        val64 = newval64;
        return 0;
    }
    return OS_ENOENT;
}

int
ctest_handle_commit(void)
{
    test_commit_called = 1;
    return 0;
}

int
ctest_handle_export(void (*cb)(char *name, char *value),
  enum conf_export_tgt tgt)
{
    char value[32];

    if (test_export_block) {
        return 0;
    }
    conf_str_from_value(CONF_INT8, &val8, value, sizeof(value));
    cb("myfoo/mybar", value);

    conf_str_from_value(CONF_INT64, &val64, value, sizeof(value));
    cb("myfoo/mybar64", value);

    return 0;
}

void
ctest_clear_call_state(void)
{
    test_get_called = 0;
    test_set_called = 0;
    test_commit_called = 0;
}

int
ctest_get_call_state(void)
{
    return test_get_called + test_set_called + test_commit_called;
}

const struct nffs_area_desc config_nffs[] = {
    { 0x00000000, 16 * 1024 },
    { 0x00004000, 16 * 1024 },
    { 0x00008000, 16 * 1024 },
    { 0x0000c000, 16 * 1024 },
    { 0, 0 }
};

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

void config_test_fill_area(char test_value[64][CONF_MAX_VAL_LEN],
  int iteration)
{
      int i, j;

      for (j = 0; j < 64; j++) {
          for (i = 0; i < CONF_MAX_VAL_LEN; i++) {
              test_value[j][i] = ((j * 2) + i + iteration) % 10 + '0';
          }
          test_value[j][sizeof(test_value[j]) - 1] = '\0';
      }
}

static void
conf_test_nffs_pre_test(void *arg)
{
    int rc;

    rc = conf_register(&config_test_handler);
    TEST_ASSERT_FATAL(rc == 0);
}

TEST_SUITE(config_test_all)
{
    tu_config.pre_test_cb = conf_test_nffs_pre_test;

    config_empty_lookups();
    config_test_getset_unknown();
    config_test_getset_int();
    config_test_getset_bytes();
    config_test_getset_int64();

    config_test_commit();

    config_setup_nffs();
    config_test_empty_file();
    config_test_small_file();
    config_test_multiple_in_file();

    config_test_save_in_file();

    config_test_save_one_file();
    config_test_get_stored_file();
}

int
main(int argc, char **argv)
{
    config_test_all();
    return tu_any_failed;
}
