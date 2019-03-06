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
#ifndef __CBMEM_TEST_H
#define __CBMEM_TEST_H

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "cbmem/cbmem.h"

#define CBMEM1_BUF_SIZE (64 * 1024)
#define CBMEM1_ENTRY_SIZE 1024

struct cbmem cbmem1;
uint8_t cbmem1_buf[CBMEM1_BUF_SIZE];
uint8_t cbmem1_entry[CBMEM1_ENTRY_SIZE];

#ifdef __cplusplus
extern "C" {
#endif

int cbmem_test_case_1_walk(struct cbmem *cbmem,
                           struct cbmem_entry_hdr *hdr, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _CBMEM_TEST_H */
