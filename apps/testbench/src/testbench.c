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

#include "os/mynewt.h"
#include "bsp/bsp.h"
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#if MYNEWT_VAL(SHELL_TASK)
#include <shell/shell.h>
#endif
#include <log/log.h>
#include <modlog/modlog.h>
#include <stats/stats.h>
#include <config/config.h>
#include "flash_map/flash_map.h"
#include <hal/hal_system.h>
#if MYNEWT_VAL(SPLIT_LOADER)
#include "split/split.h"
#endif
#include <bootutil/image.h>
#include <bootutil/bootutil.h>
#include <imgmgr/imgmgr.h>
#include <assert.h>
#include <string.h>
#include <json/json.h>
#include <reboot/log_reboot.h>
#include <id/id.h>
#include <oic/oc_api.h>
#include <oic/oc_gatt.h>
#include "json_test/json_test.h"
#include "cbmem_test/cbmem_test.h"

#include "testutil/testutil.h"

#if MYNEWT_VAL(RUNTEST_CLI)
#include "runtest/runtest.h"
#endif
#include "tbb.h"

struct os_timeval tv;
struct os_timezone tz;

/* Test Task */
#define TESTTASK_PRIO (1)
#define TESTTASK_STACK_SIZE    OS_STACK_ALIGN(256)
static struct os_task testtask;

/* For LED toggling */
int g_led_pin;

int blinky_blink;

#define BLINKY_DUTYCYCLE_SUCCESS 1
#define BLINKY_DUTYCYCLE_FAIL 16

OS_TASK_STACK_DEFINE(teststack, TESTTASK_STACK_SIZE);

void
testbench_test_init(void)
{
    blinky_blink = BLINKY_DUTYCYCLE_SUCCESS;
}

/*
 * Run the tests
 * If any tests fail, blink the LED BLINKY_DUTYCYCLE_FAIL (16) times a second
 */
static void
testtask_handler(void *arg)
{
    os_gettimeofday(&tv, &tz);

    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    while (1) {
        /*
         * if any test fails, blinky the LED more rapidly to
         * provide visual feedback from physical device.
         */
        if (runtest_total_fails_get() > 0) {
            blinky_blink = BLINKY_DUTYCYCLE_FAIL;
        }

        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC / blinky_blink);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);
    }
}

static void
omgr_app_init(void)
{ }

static const oc_handler_t omgr_oc_handler = {
    .init = omgr_app_init,
};

/*
 * main()
 * Keep this app simple, just run the tests and then report success or failure.
 * Complexity is pushed down to the individual test suites and component test cases.
 */
int
main(int argc, char **argv)
{
    int rc;

    sysinit();

    /* Initialize the OIC  */
    oc_main_init((oc_handler_t *)&omgr_oc_handler);

#if MYNEWT_VAL(TESTBENCH_BLE)
    tbb_init();
    oc_ble_coap_gatt_srv_init();
#endif

    reboot_start(hal_reset_cause());

    /*
     * Register the tests that can be run by lookup
     * - each test is added to the ts_suites slist
     */
    TEST_SUITE_REGISTER(cbmem_test_suite);
    TEST_SUITE_REGISTER(test_json_suite);

    testbench_test_init(); /* initialize globals include blink duty cycle */

    os_task_init(&testtask, "testtask", testtask_handler, NULL,
                 TESTTASK_PRIO, OS_WAIT_FOREVER, teststack,
                 TESTTASK_STACK_SIZE);

    MODLOG_INFO(LOG_MODULE_TEST, "testbench app initialized");

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);

    return rc;
}
