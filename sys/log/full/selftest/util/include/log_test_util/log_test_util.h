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

#ifndef H_LOG_TEST_UTIL_
#define H_LOG_TEST_UTIL_

#include "os/mynewt.h"
#include "testutil/testutil.h"
#if MYNEWT_VAL(LOG_FCB)
#include "fcb/fcb.h"
#elif MYNEWT_VAL(LOG_FCB2)
#include "fcb/fcb2.h"
#endif
#include "log/log.h"
#include "log_test_util.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MYNEWT_VAL(LOG_FCB)
extern struct fcb log_fcb;
#elif MYNEWT_VAL(LOG_FCB2)
extern struct fcb2 log_fcb;
#endif
extern struct log my_log;
extern char *ltu_str_logs[];
extern uint8_t dummy_log_arr[];

struct os_mbuf *ltu_flat_to_fragged_mbuf(const void *flat, int len,
                                         int frag_sz);
void ltu_setup_fcb(struct fcb_log *fcb_log, struct log *log);
void ltu_setup_2fcbs(struct fcb_log *fcb_log1, struct log *log1,
                     struct fcb_log *fcb_log2, struct log *log2);
void ltu_setup_cbmem(struct cbmem *cbmem, struct log *log);
void ltu_verify_contents(struct log *log);
uint16_t *ltu_get_ltu_off_arr(void);
uint16_t ltu_init_arr(void);
int ltu_num_strs(void);

TEST_SUITE_DECL(log_test_suite_cbmem_flat);
TEST_CASE_DECL(log_test_case_cbmem_append);
TEST_CASE_DECL(log_test_case_cbmem_append_body);
TEST_CASE_DECL(log_test_case_cbmem_printf);

TEST_SUITE_DECL(log_test_suite_cbmem_mbuf);
TEST_CASE_DECL(log_test_case_cbmem_append_mbuf);
TEST_CASE_DECL(log_test_case_cbmem_append_mbuf_body);

TEST_SUITE_DECL(log_test_suite_fcb_flat);
TEST_CASE_DECL(log_test_case_fcb_append);
TEST_CASE_DECL(log_test_case_fcb_append_body);
TEST_CASE_DECL(log_test_case_fcb_printf);

TEST_SUITE_DECL(log_test_suite_fcb_mbuf);
TEST_CASE_DECL(log_test_case_fcb_append_mbuf);
TEST_CASE_DECL(log_test_case_fcb_append_mbuf_body);

TEST_SUITE_DECL(log_test_suite_misc);
TEST_CASE_DECL(log_test_case_level);
TEST_CASE_DECL(log_test_case_append_cb);

TEST_CASE_DECL(log_test_case_2logs);

#ifdef __cplusplus
}
#endif

#endif
