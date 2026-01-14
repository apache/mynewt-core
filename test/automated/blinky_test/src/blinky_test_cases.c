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

#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "blinky_test/blinky_test.h"

MTEST_CASE(blinky_test_case_1)
{
    hal_gpio_write(LED_BLINK_PIN, 1);

    os_time_delay(OS_TICKS_PER_SEC / 10);

    int state = hal_gpio_read(MYNEWT_VAL(LED_READ_PIN));

    MTEST_CASE_ASSERT(state == 1, "GPIO loopback HIGH failed");
}

MTEST_CASE(blinky_test_case_2)
{
    hal_gpio_write(LED_BLINK_PIN, 0);

    os_time_delay(OS_TICKS_PER_SEC / 10);

    int state = hal_gpio_read(MYNEWT_VAL(LED_READ_PIN));

    MTEST_CASE_ASSERT(state == 0, "GPIO loopback LOW failed");
}

MTEST_CASE(blinky_test_case_3)
{
    int out = 0;

    for (int i = 0; i < 10; i++) {

        out = !out;

        hal_gpio_write(LED_BLINK_PIN, out);

        os_time_delay(OS_TICKS_PER_SEC / 10);

        int in = hal_gpio_read(MYNEWT_VAL(LED_READ_PIN));

        MTEST_CASE_ASSERT(in == out, "GPIO loopback toggle failed");
    }
}
