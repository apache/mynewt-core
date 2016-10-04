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

#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include "syscfg/syscfg.h"
#include <flash_map/flash_map.h>
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_bsp.h>
#include <hal/hal_system.h>
#include <hal/hal_flash.h>
#if MYNEWT_VAL(BOOT_SERIAL)
#include <hal/hal_gpio.h>
#include <boot_serial/boot_serial.h>
#endif
#include <console/console.h>
#include "bootutil/image.h"
#include "bootutil/loader.h"
#include "bootutil/bootutil_misc.h"

#define BOOT_AREA_DESC_MAX  (256)
#define AREA_DESC_MAX       (BOOT_AREA_DESC_MAX)

#if MYNEWT_VAL(BOOT_SERIAL)
#define BOOT_SER_PRIO_TASK          1
#define BOOT_SER_STACK_SZ           512
#define BOOT_SER_CONS_INPUT         128

static struct os_task boot_ser_task;
static os_stack_t boot_ser_stack[BOOT_SER_STACK_SZ];
#endif

int
main(void)
{
    struct flash_area descs[AREA_DESC_MAX];
    /** Areas representing the beginning of image slots. */
    uint8_t img_starts[2];
    struct boot_req req = {
        .br_area_descs = descs,
        .br_slot_areas = img_starts,
    };

    struct boot_rsp rsp;
    int rc;

#if MYNEWT_VAL(BOOT_SERIAL)
    sysinit();
#else
    flash_map_init();
    bsp_init();
#endif

    rc = boot_build_request(&req, AREA_DESC_MAX);
    assert(rc == 0);

#if MYNEWT_VAL(BOOT_SERIAL)
    /*
     * Configure a GPIO as input, and compare it against expected value.
     * If it matches, await for download commands from serial.
     */
    hal_gpio_init_in(BOOT_SERIAL_DETECT_PIN, BOOT_SERIAL_DETECT_PIN_CFG);
    if (hal_gpio_read(BOOT_SERIAL_DETECT_PIN) == BOOT_SERIAL_DETECT_PIN_VAL) {
        rc = boot_serial_task_init(&boot_ser_task, BOOT_SER_PRIO_TASK,
          boot_ser_stack, BOOT_SER_STACK_SZ, BOOT_SER_CONS_INPUT);
        assert(rc == 0);
        os_start();
    }
#endif
    rc = boot_go(&req, &rsp);
    assert(rc == 0);

    system_start((void *)(rsp.br_image_addr + rsp.br_hdr->ih_hdr_size));

    return 0;
}
