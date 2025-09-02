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
#ifndef __CRASH_TEST_PRIV_H__
#define __CRASH_TEST_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

#if MYNEWT_VAL(CRASH_TEST_MGMT)
extern struct mgmt_group crash_test_mgmt_group;
#endif

int crash_device(char *how);
int crash_verify_cmd(char *how);

#ifdef __cplusplus
}
#endif

#endif /* __CRASH_TEST_PRIV_H__ */
