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
#include <fcb/fcb.h>
#include "config/config.h"
#include "config/config_file.h"
#include "config/config_fcb.h"
#include "test/config_test.h"
#include "config/../../src/config_priv.h"

static uint8_t val8;
int c2_var_count = 1;
static char val_string[64][CONF_MAX_VAL_LEN];

static uint32_t val32;

static int test_get_called;
static int test_set_called;
static int test_commit_called;
static int test_export_block;

static char *ctest_handle_get(int argc, char **argv, char *val,
  int val_len_max);
static int ctest_handle_set(int argc, char **argv, char *val);
static int ctest_handle_commit(void);
static int ctest_handle_export(void (*cb)(char *name, char *value),
  enum conf_export_tgt tgt);
static char *c2_handle_get(int argc, char **argv, char *val,
  int val_len_max);
static int c2_handle_set(int argc, char **argv, char *val);
static int c2_handle_export(void (*cb)(char *name, char *value),
  enum conf_export_tgt tgt);
static char *c3_handle_get(int argc, char **argv, char *val,
  int val_len_max);
static int c3_handle_set(int argc, char **argv, char *val);
static int c3_handle_export(void (*cb)(char *name, char *value),
  enum conf_export_tgt tgt);

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
ctest_handle_export(void (*cb)(char *name, char *value),
  enum conf_export_tgt tgt)
{
    char value[32];

    if (test_export_block) {
        return 0;
    }
    conf_str_from_value(CONF_INT8, &val8, value, sizeof(value));
    cb("myfoo/mybar", value);

    return 0;
}

struct conf_handler c2_test_handler = {
    .ch_name = "2nd",
    .ch_get = c2_handle_get,
    .ch_set = c2_handle_set,
    .ch_commit = NULL,
    .ch_export = c2_handle_export
};

char *
c2_var_find(char *name)
{
    int idx = 0;
    int len;
    char *eptr;

    len = strlen(name);
    TEST_ASSERT(!strncmp(name, "string", 6));
    TEST_ASSERT(len > 6);

    idx = strtoul(&name[6], &eptr, 10);
    TEST_ASSERT(*eptr == '\0');
    TEST_ASSERT(idx < c2_var_count);
    return val_string[idx];
}

static char *
c2_handle_get(int argc, char **argv, char *val, int val_len_max)
{
    int len;
    char *valptr;

    if (argc == 1) {
        valptr = c2_var_find(argv[0]);
        if (!valptr) {
            return NULL;
        }
        len = strlen(val_string[0]);
        if (len > val_len_max) {
            len = val_len_max;
        }
        strncpy(val, valptr, len);
    }
    return NULL;
}

static int
c2_handle_set(int argc, char **argv, char *val)
{
    char *valptr;

    if (argc == 1) {
        valptr = c2_var_find(argv[0]);
        if (!valptr) {
            return OS_ENOENT;
        }
        if (val) {
            strncpy(valptr, val, sizeof(val_string[0]));
        } else {
            memset(valptr, 0, sizeof(val_string[0]));
        }
        return 0;
    }
    return OS_ENOENT;
}

static int
c2_handle_export(void (*cb)(char *name, char *value),
  enum conf_export_tgt tgt)
{
    int i;
    char name[32];

    for (i = 0; i < c2_var_count; i++) {
        snprintf(name, sizeof(name), "2nd/string%d", i);
        cb(name, val_string[i]);
    }
    return 0;
}

struct conf_handler c3_test_handler = {
    .ch_name = "3",
    .ch_get = c3_handle_get,
    .ch_set = c3_handle_set,
    .ch_commit = NULL,
    .ch_export = c3_handle_export
};

static char *
c3_handle_get(int argc, char **argv, char *val, int val_len_max)
{
    if (argc == 1 && !strcmp(argv[0], "v")) {
        return conf_str_from_value(CONF_INT32, &val32, val, val_len_max);
    }
    return NULL;
}

static int
c3_handle_set(int argc, char **argv, char *val)
{
    uint32_t newval;
    int rc;

    if (argc == 1 && !strcmp(argv[0], "v")) {
        rc = CONF_VALUE_SET(val, CONF_INT32, newval);
        TEST_ASSERT(rc == 0);
        val32 = newval;
        return 0;
    }
    return OS_ENOENT;
}

static int
c3_handle_export(void (*cb)(char *name, char *value),
  enum conf_export_tgt tgt)
{
    char value[32];

    conf_str_from_value(CONF_INT32, &val32, value, sizeof(value));
    cb("3/v", value);

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

TEST_CASE(config_test_insert2)
{
    int rc;

    rc = conf_register(&c2_test_handler);
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

TEST_CASE(config_test_getset_bytes)
{
    char orig[32];
    char bytes[32];
    char str[48];
    char *ret;
    int j, i;
    int tmp;
    int rc;

    for (j = 1; j < sizeof(orig); j++) {
        for (i = 0; i < j; i++) {
            orig[i] = i + j + 1;
        }
        ret = conf_str_from_bytes(orig, j, str, sizeof(str));
        TEST_ASSERT(ret);
        tmp = strlen(str);
        TEST_ASSERT(tmp < sizeof(str));

        memset(bytes, 0, sizeof(bytes));
        tmp = sizeof(bytes);

        tmp = sizeof(bytes);
        rc = conf_bytes_from_str(str, bytes, &tmp);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(tmp == j);
        TEST_ASSERT(!memcmp(orig, bytes, j));
    }
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

static void config_wipe_fcb(struct flash_area *fa, int cnt)
{
    int i;

    for (i = 0; i < cnt; i++) {
        flash_area_erase(&fa[i], 0, fa[i].fa_size);
    }
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

TEST_CASE(config_test_save_one_file)
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

struct flash_area fcb_areas[] = {
    [0] = {
        .fa_off = 0x00000000,
        .fa_size = 16 * 1024
    },
    [1] = {
        .fa_off = 0x00004000,
        .fa_size = 16 * 1024
    },
    [2] = {
        .fa_off = 0x00008000,
        .fa_size = 16 * 1024
    },
    [3] = {
        .fa_off = 0x0000c000,
        .fa_size = 16 * 1024
    }
};

TEST_CASE(config_test_empty_fcb)
{
    int rc;
    struct conf_fcb cf;

    config_wipe_srcs();
    config_wipe_fcb(fcb_areas, sizeof(fcb_areas) / sizeof(fcb_areas[0]));

    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    /*
     * No values
     */
    conf_load();

    config_wipe_srcs();
    ctest_clear_call_state();
}

TEST_CASE(config_test_save_1_fcb)
{
    int rc;
    struct conf_fcb cf;

    config_wipe_srcs();

    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb_dst(&cf);
    TEST_ASSERT(rc == 0);

    val8 = 33;
    rc = conf_save();
    TEST_ASSERT(rc == 0);

    val8 = 0;

    rc = conf_load();
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(val8 == 33);
}

static void config_test_fill_area(char test_value[64][CONF_MAX_VAL_LEN],
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

TEST_CASE(config_test_save_2_fcb)
{
    int rc;
    struct conf_fcb cf;
    char test_value[64][CONF_MAX_VAL_LEN];
    int i;

    config_wipe_srcs();

    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb_dst(&cf);
    TEST_ASSERT(rc == 0);

    config_test_fill_area(test_value, 0);
    memcpy(val_string, test_value, sizeof(val_string));

    val8 = 42;
    rc = conf_save();
    TEST_ASSERT(rc == 0);

    val8 = 0;
    memset(val_string[0], 0, sizeof(val_string[0]));
    rc = conf_load();
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(val8 == 42);
    TEST_ASSERT(!strcmp(val_string[0], test_value[0]));
    test_export_block = 1;

    /*
     * Now add the number of settings to max. Keep adjusting the test_data,
     * check that rollover happens when it's supposed to.
     */
    c2_var_count = 64;

    for (i = 0; i < 32; i++) {
        config_test_fill_area(test_value, i);
        memcpy(val_string, test_value, sizeof(val_string));

        rc = conf_save();
        TEST_ASSERT(rc == 0);

        memset(val_string, 0, sizeof(val_string));

        val8 = 0;
        rc = conf_load();
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(!memcmp(val_string, test_value, sizeof(val_string)));
        TEST_ASSERT(val8 == 42);
    }
    c2_var_count = 0;
}

TEST_CASE(config_test_insert3)
{
    int rc;

    rc = conf_register(&c3_test_handler);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(config_test_save_3_fcb)
{
    int rc;
    struct conf_fcb cf;
    int i;

    config_wipe_srcs();
    config_wipe_fcb(fcb_areas, sizeof(fcb_areas) / sizeof(fcb_areas[0]));

    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = 4;

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb_dst(&cf);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < 4096; i++) {
        val32 = i;

        rc = conf_save();
        TEST_ASSERT(rc == 0);

        val32 = 0;

        rc = conf_load();
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(val32 == i);
    }
}

TEST_CASE(config_test_compress_reset)
{
    int rc;
    struct conf_fcb cf;
    struct flash_area *fa;
    char test_value[64][CONF_MAX_VAL_LEN];
    int elems[4];
    int i;

    config_wipe_srcs();
    config_wipe_fcb(fcb_areas, sizeof(fcb_areas) / sizeof(fcb_areas[0]));

    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb_dst(&cf);
    TEST_ASSERT(rc == 0);

    c2_var_count = 1;
    memset(elems, 0, sizeof(elems));

    for (i = 0; ; i++) {
        config_test_fill_area(test_value, i);
        memcpy(val_string, test_value, sizeof(val_string));

        rc = conf_save();
        TEST_ASSERT(rc == 0);

        if (cf.cf_fcb.f_active.fe_area == &fcb_areas[2]) {
            /*
             * Started using space just before scratch.
             */
            break;
        }
        memset(val_string, 0, sizeof(val_string));

        rc = conf_load();
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(!memcmp(val_string, test_value, CONF_MAX_VAL_LEN));
    }

    fa = cf.cf_fcb.f_active.fe_area;
    rc = fcb_append_to_scratch(&cf.cf_fcb);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(fcb_free_sector_cnt(&cf.cf_fcb) == 0);
    TEST_ASSERT(fa != cf.cf_fcb.f_active.fe_area);

    config_wipe_srcs();

    memset(&cf, 0, sizeof(cf));

    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb_dst(&cf);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(fcb_free_sector_cnt(&cf.cf_fcb) == 1);
    TEST_ASSERT(fa == cf.cf_fcb.f_active.fe_area);

    c2_var_count = 0;
}

TEST_CASE(config_test_save_one_fcb)
{
    int rc;
    struct conf_fcb cf;

    config_wipe_srcs();
    config_wipe_fcb(fcb_areas, sizeof(fcb_areas) / sizeof(fcb_areas[0]));

    cf.cf_fcb.f_sectors = fcb_areas;
    cf.cf_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);

    rc = conf_fcb_src(&cf);
    TEST_ASSERT(rc == 0);

    rc = conf_fcb_dst(&cf);
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

TEST_SUITE(config_test_all)
{
    /*
     * Config tests.
     */
    config_empty_lookups();
    config_test_insert();
    config_test_getset_unknown();
    config_test_getset_int();
    config_test_getset_bytes();

    config_test_commit();

    /*
     * NFFS as backing storage.
     */
    config_setup_nffs();
    config_test_empty_file();
    config_test_small_file();
    config_test_multiple_in_file();

    config_test_save_in_file();

    config_test_save_one_file();

    /*
     * FCB as backing storage.
     */
    config_test_empty_fcb();
    config_test_save_1_fcb();

    config_test_insert2();

    config_test_save_2_fcb();

    config_test_insert3();
    config_test_save_3_fcb();

    config_test_compress_reset();

    config_test_save_one_fcb();
}

