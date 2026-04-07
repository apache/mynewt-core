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

#include "mtest/mtest.h"
#include "mtest_nvreg/mtest_nvreg.h"

MTEST_CASE(nvreg_test_case_1)
{
    struct nvreg_err_dump err_dump;
    int rc;

    rc = nvregs_compare_zero(&err_dump);
    MTEST_CASE_ASSERT(rc == 0, "reg[%d]=0x%lx, expected reg[%d]=0",
                      err_dump.reg, err_dump.val, err_dump.reg);

    nvreg_set_pattern();
}

MTEST_CASE(nvreg_test_case_2)
{
    struct nvreg_err_dump err_dump;
    int rc;

    rc = nvregs_compare_pattern(&err_dump);
    MTEST_CASE_ASSERT(rc == 0, "pattern mismatch reg[%d]=0x%lx, expected reg[%d]=0x%lx",
                      err_dump.reg, err_dump.val, err_dump.reg, err_dump.exp_val);

    nvregs_set_zero();
}

MTEST_CASE(nvreg_test_case_3)
{
    struct nvreg_err_dump err_dump;
    int rc;

    rc = nvregs_compare_zero(&err_dump);
    MTEST_CASE_ASSERT(rc == 0, "reg[%d]=0x%lx, expected reg[%d]=0",
                      err_dump.reg, err_dump.val, err_dump.reg);
}
