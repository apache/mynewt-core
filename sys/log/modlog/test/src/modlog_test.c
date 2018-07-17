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

#include "os/mynewt.h"
#include "modlog_test.h"

TEST_SUITE(modlog_test_suite_all)
{
    modlog_test_case_append();
    modlog_test_case_basic();
    modlog_test_case_printf();
    modlog_test_case_prio();
}

#if MYNEWT_VAL(SELFTEST)

int
main(int argc, char **argv)
{
    sysinit();

    modlog_test_suite_all();

    return tu_any_failed;
}

#endif
