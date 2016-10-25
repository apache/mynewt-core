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

#ifndef H_TESTUTIL_PRIV_
#define H_TESTUTIL_PRIV_

#include <stddef.h>
#include <inttypes.h>

#include "testutil/testutil.h"

#ifdef __cplusplus
extern "C" {
#endif

void tu_arch_restart(void);
void tu_case_abort(void);

extern tu_post_test_fn_t *tu_case_post_test_cb;
extern void *tu_case_post_test_cb_arg;

#ifdef __cplusplus
}
#endif

#endif
