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
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "blinky_test/blinky_test.h"

MTEST_INIT(blinky_test)
{
    int rc = hal_gpio_init_out(LED_BLINK_PIN, 0);
    MTEST_INIT_ASSERT(rc == 0, "GPIO output init failed");

    rc = hal_gpio_init_in(MYNEWT_VAL(LED_READ_PIN), HAL_GPIO_PULL_DOWN);
    MTEST_INIT_ASSERT(rc == 0, "GPIO input init failed");
}

MTEST_CLEANUP(blinky_test)
{
    int rc = hal_gpio_deinit(LED_BLINK_PIN);
    MTEST_CLEANUP_ASSERT(rc == 0, "GPIO output deinit failed");

    rc = hal_gpio_deinit(MYNEWT_VAL(LED_READ_PIN));
    MTEST_CLEANUP_ASSERT(rc == 0, "GPIO input deinit failed");
}

MTEST_SUITE(blinky_test)
{
    MTEST_RUN_INIT(blinky_test);
    blinky_test_case_1();
    blinky_test_case_2();
    blinky_test_case_3();
    MTEST_RUN_CLEANUP(blinky_test);
}

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    blinky_test();

    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
    }
}
