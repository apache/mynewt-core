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
#include <stddef.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"

TEST_CASE_DECL(tinyprintf_test)
TEST_CASE_DECL(strndup_test)
TEST_CASE_DECL(strnstr_test)
TEST_CASE_DECL(sscanf_set_test)

TEST_SUITE(baselibc_test_suite)
{
    tinyprintf_test();
    strndup_test();
    strnstr_test();
    sscanf_set_test();
}

int
main(int argc, char **argv)
{
    baselibc_test_suite();

    return tu_any_failed;
}
