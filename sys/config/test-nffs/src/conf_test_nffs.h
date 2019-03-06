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
#ifndef _CONF_TEST_H_
#define _CONF_TEST_H_

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

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t val8;
extern int c2_var_count;
extern char val_string[64][CONF_MAX_VAL_LEN];

extern uint32_t val32;
extern uint64_t val64;

extern int test_get_called;
extern int test_set_called;
extern int test_commit_called;
extern int test_export_block;

int ctest_get_call_state(void);
void ctest_clear_call_state(void);

int conf_test_file_strstr(const char *fname, char *string);

TEST_CASE_DECL(config_empty_lookups);
TEST_CASE_DECL(config_test_getset_unknown);
TEST_CASE_DECL(config_test_getset_int);
TEST_CASE_DECL(config_test_getset_bytes);
TEST_CASE_DECL(config_test_getset_int64);
TEST_CASE_DECL(config_test_commit);
TEST_CASE_DECL(config_setup_nffs);
TEST_CASE_DECL(config_test_empty_file);
TEST_CASE_DECL(config_test_small_file);;
TEST_CASE_DECL(config_test_multiple_in_file);
TEST_CASE_DECL(config_test_save_in_file);
TEST_CASE_DECL(config_test_save_one_file);
TEST_CASE_DECL(config_test_get_stored_file);

extern struct conf_handler config_test_handler;
extern const struct nffs_area_desc config_nffs[];

#ifdef __cplusplus
}
#endif

#endif /* _CONF_TEST_H_ */
