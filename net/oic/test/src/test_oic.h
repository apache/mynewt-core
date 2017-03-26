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
#ifndef TEST_OIC_H
#define TEST_OIC_H

#include <assert.h>
#include <string.h>
#include "testutil/testutil.h"
#include "test_oic.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Testcases
 */
TEST_CASE_DECL(oic_tests);

extern struct os_eventq oic_tapp_evq;

void oic_test_reset_tmo(const char *phase);
struct oc_server_handle;
void oic_test_set_endpoint(struct oc_server_handle *);
void oic_test_get_endpoint(struct oc_server_handle *);

void test_discovery(void);
void test_getset(void);
void test_observe(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_OIC_H */

