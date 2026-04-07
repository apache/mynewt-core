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

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "mtest/mtest.h"
#include "mtest_nvreg/mtest_nvreg.h"

#define FIRST_BOOT_DONE 0xC001B007
int reset_count __attribute__((section(".noinit")));
int boot_status __attribute__((section(".noinit")));

MTEST_INIT(nvreg_test)
{
    if (boot_status != FIRST_BOOT_DONE) {
        reset_count = 0;
        boot_status = FIRST_BOOT_DONE;
    }
}

MTEST_SUITE(nvreg_test)
{
    MTEST_RUN_INIT(nvreg_test);

    switch (reset_count) {
    case 0:
        nvreg_test_case_1();
        nvregs_reset();
        break;

    case 1:
        nvreg_test_case_2();
        nvregs_reset();
        break;

    case 2:
        nvreg_test_case_3();
        nvregs_reset();
        break;

    default:
        boot_status = 0;
        reset_count = 0;
        break;
    }
}

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    nvreg_test();

    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
    }
}
