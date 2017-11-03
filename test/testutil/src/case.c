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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "testutil/testutil.h"
#include "testutil_priv.h"

jmp_buf tu_case_jb;
int tu_case_reported;
int tu_case_failed;
int tu_case_idx;

const char *tu_case_name;

#define TU_CASE_BUF_SZ      256

static char tu_case_buf[TU_CASE_BUF_SZ];
static int tu_case_buf_len;

void
tu_case_abort(void)
{
    tu_case_write_pass_auto();
    longjmp(tu_case_jb, 1);
}

static int
tu_case_vappend_buf(const char *format, va_list ap)
{
    char *dst;
    int maxlen;
    int rc;

    dst = tu_case_buf + tu_case_buf_len;
    maxlen = TU_CASE_BUF_SZ - tu_case_buf_len;

    rc = vsnprintf(dst, maxlen, format, ap);
    tu_case_buf_len += rc;
    if (tu_case_buf_len >= TU_CASE_BUF_SZ) {
        tu_case_buf_len = TU_CASE_BUF_SZ - 1;
        return -1;
    }

    return 0;
}

static int
tu_case_append_buf(const char *format, ...)
{
    va_list ap;
    int rc;

    va_start(ap, format);
    rc = tu_case_vappend_buf(format, ap);
    va_end(ap);

    return rc;
}

static void
tu_case_set_name(const char *name)
{
    tu_case_name = name;
}

/**
 * Configures a callback that gets executed at the end of each test.
 * This callback is cleared when the current test completes.
 *
 * @param cb -  The callback to execute at the end of each test case.
 * @param cb_arg - An optional argument that gets passed to the
 *                                  callback.
 */
/*
 * Called to initialize test (after tu_suite_pre_cb and before 
 */
void
tu_case_set_init_cb(tu_init_test_fn_t *cb, void *cb_arg)
{
    tc_config.tc_case_init_cb = cb;
    tc_config.tc_case_init_arg = cb_arg;
}

void
tu_case_set_pre_cb(tu_pre_test_fn_t *cb, void *cb_arg)
{
    tc_config.tc_case_pre_test_cb = cb;
    tc_config.tc_case_pre_arg = cb_arg;
}

void
tu_case_set_post_cb(tu_post_test_fn_t *cb, void *cb_arg)
{
    tc_config.tc_case_post_test_cb = cb;
    tc_config.tc_case_post_arg = cb_arg;
}

void
tu_case_init(const char *name)
{
    tu_case_reported = 0;
    tu_case_failed = 0;

    tu_case_set_name(name);

    if (tc_config.tc_case_init_cb != NULL) {
        tc_config.tc_case_init_cb(tc_config.tc_case_init_arg);
    }
}

void
tu_case_complete(void)
{
    tu_case_idx++;
    tu_case_set_pre_cb(NULL, NULL);
    tu_case_set_post_cb(NULL, NULL);
}

void
tu_case_pre_test(void)
{
    if (tc_config.tc_case_pre_test_cb != NULL) {
        tc_config.tc_case_pre_test_cb(tc_config.tc_case_pre_arg);
    }
}

void
tu_case_post_test(void)
{
    if (tc_config.tc_case_post_test_cb != NULL) {
        tc_config.tc_case_post_test_cb(tc_config.tc_case_post_arg);
    }
}

static void
tu_case_buf_clear(void)
{
    tu_case_buf_len = 0;
    tu_case_buf[0] = '\0';
}

static void
tu_case_write_pass_buf(void)
{
#if MYNEWT_VAL(SELFTEST)
    if (ts_config.ts_print_results) {
        printf("[pass] %s/%s %s\n", ts_current_config->ts_suite_name,
               tu_case_name, tu_case_buf);
        fflush(stdout);
    }
#endif

    tu_case_reported = 1;

    if (ts_config.ts_case_pass_cb != NULL) {
        ts_config.ts_case_pass_cb(tu_case_buf, ts_config.ts_case_pass_arg);
    }

    tu_case_buf_clear();
}

void
tu_case_pass(void)
{
    tu_case_write_pass_buf();
    tu_case_reported = 1;
    tu_case_failed = 0;
}

void
tu_case_fail(void)
{
    tu_case_reported = 1;
    tu_case_failed = 1;
    tu_suite_failed = 1;
    tu_any_failed = 1;

#if MYNEWT_VAL(SELFTEST)
    if (ts_config.ts_print_results) {
        printf("[FAIL] %s/%s %s", ts_current_config->ts_suite_name,
               tu_case_name, tu_case_buf);
        fflush(stdout);
    }
#endif

    tu_case_post_test();

    if (ts_config.ts_case_fail_cb != NULL) {
        ts_config.ts_case_fail_cb(tu_case_buf, ts_config.ts_case_fail_arg);
    }

    tu_case_buf_clear();
}

static void
tu_case_append_file_info(const char *file, int line)
{
    int rc;

    rc = tu_case_append_buf("|%s:%d| ", file, line);
    assert(rc == 0);
}

static void
tu_case_append_assert_msg(const char *expr)
{
    int rc;

    rc = tu_case_append_buf("failed assertion: %s", expr);
    assert(rc == 0);
}

static void
tu_case_append_manual_pass_msg(void)
{
    int rc;

    rc = tu_case_append_buf("manual pass");
    assert(rc == 0);
}

void
tu_case_write_pass_auto(void)
{
    if (!tu_case_reported) {
        tu_case_write_pass_buf();
        tu_case_reported = 1;
    }
}

void
tu_case_fail_assert(int fatal, const char *file, int line,
                    const char *expr, const char *format, ...)
{
    va_list ap;
    int rc;

    if (ts_config.ts_system_assert) {
        assert(0);
    }

    tu_case_buf_len = 0;

    tu_case_append_file_info(file, line);
    tu_case_append_assert_msg(expr);

    if (format != NULL) {
        rc = tu_case_append_buf("\n");
        assert(rc == 0);

        va_start(ap, format);
        rc = tu_case_vappend_buf(format, ap);
        assert(rc == 0);
        va_end(ap);
    }

    rc = tu_case_append_buf("\n");
    assert(rc == 0);

    tu_case_fail();

    if (fatal) {
        tu_case_abort();
    }
}

void
tu_case_pass_manual(const char *file, int line, const char *format, ...)
{
    va_list ap;
    int rc;

    if (tu_case_reported) {
        return;
    }

    tu_case_append_file_info(file, line);
    tu_case_append_manual_pass_msg();

    if (format != NULL) {
        rc = tu_case_append_buf("\n");
        assert(rc == 0);

        va_start(ap, format);
        rc = tu_case_vappend_buf(format, ap);
        assert(rc == 0);
        va_end(ap);
    }

    rc = tu_case_append_buf("\n");
    assert(rc == 0);

    tu_case_write_pass_buf();

    tu_case_abort();
}
