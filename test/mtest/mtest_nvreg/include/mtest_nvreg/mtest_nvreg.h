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

#ifndef MTEST_NVMREG_H
#define MTEST_NVMREG_H
#include "mtest/mtest.h"

#ifdef __cplusplus
extern "C" {
#endif

MTEST_CASE_DECL(nvreg_test_case_1);
MTEST_CASE_DECL(nvreg_test_case_2);
MTEST_CASE_DECL(nvreg_test_case_3);

struct nvreg_err_dump {
    int reg;
    uint32_t val;
    uint32_t exp_val;
};

void nvregs_reset(void);
void nvregs_set_zero(void);
int nvregs_compare_zero(struct nvreg_err_dump *err);
void nvreg_set_pattern(void);
int nvregs_compare_pattern(struct nvreg_err_dump *err);

#ifdef __cplusplus
extern "C"
}
#endif

#endif
