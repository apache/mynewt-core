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


#include <os/mynewt.h>
#include <hal/hal_watchdog.h>
#include <device/usbd.h>

#include <tusb.h>

#include <tinyusb/tinyusb.h>

#define USBD_STACK_SIZE     MYNEWT_VAL(USBD_STACK_SIZE)
#define USBD_TASK_PRIORITY  MYNEWT_VAL(USBD_TASK_PRIORITY)

#if MYNEWT_VAL(OS_SCHEDULING)
static struct os_task usbd_task;
OS_TASK_STACK_DEFINE(usbd_stack, USBD_STACK_SIZE);
#endif

/**
 * USB Device Driver task
 * This top level thread process all usb events and invoke callbacks
 */
static void
tinyusb_device_task(void *param)
{
    (void)param;

    while (1) {
#if !MYNEWT_VAL(OS_SCHEDULING) && MYNEWT_VAL(WATCHDOG_INTERVAL)
        hal_watchdog_tickle();
#endif
        tud_task();
    }
}

void
tinyusb_start(void)
{
    /**
     * Note:
     * Interrupt initialization.
     * It would be nice to have this present in tinyusb repository since this duplicates code
     * present in bsp initialization there.
     */
    tinyusb_hardware_init();

    /* USB stack initialization */
    tusb_init();

#if MYNEWT_VAL(OS_SCHEDULING)
    /* Create a task for tinyusb device stack */
    os_task_init(&usbd_task, "usbd", tinyusb_device_task, NULL, USBD_TASK_PRIORITY,
                 OS_WAIT_FOREVER, usbd_stack, USBD_STACK_SIZE);
#else
    tinyusb_device_task(NULL);
#endif
}
