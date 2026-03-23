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

#include "bsp.h"
#include "console/console.h"
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "syscfg/syscfg.h"
#include "cputimer_test/cputimer_test.h"

struct cputimer_test_ctx test_ctx;

MTEST_SUITE(cputimer_test)
{
    cputimer_test_case_1();
    cputimer_test_case_2();
    cputimer_test_case_3();
    cputimer_test_case_4();
    cputimer_test_case_5();
}

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    cputimer_test();

    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
    }
}
