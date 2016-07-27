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
#include <hal/flash_map.h>
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_system.h>
#include <hal/hal_flash.h>
#include <config/config.h>
#include <config/config_file.h>
#ifdef NFFS_PRESENT
#include <fs/fs.h>
#include <nffs/nffs.h>
#elif FCB_PRESENT
#include <fcb/fcb.h>
#include <config/config_fcb.h>
#else
#error "Need NFFS or FCB for config storage"
#endif
#ifdef BOOT_SERIAL
#include <hal/hal_gpio.h>
#include <boot_serial/boot_serial.h>
#endif
#include "bootutil/image.h"
#include "bootutil/loader.h"
#include "bootutil/bootutil_misc.h"

/* we currently need extra nffs_area_descriptors for booting since the
 * boot code uses these to keep track of which block to write and copy.*/
#define BOOT_AREA_DESC_MAX  (256)
#define AREA_DESC_MAX       (BOOT_AREA_DESC_MAX)

#ifdef BOOT_SERIAL
#define BOOT_SER_PRIO_TASK          1
#define BOOT_SER_STACK_SZ           512
#define BOOT_SER_CONS_INPUT         128

static struct os_task boot_ser_task;
static os_stack_t boot_ser_stack[BOOT_SER_STACK_SZ];
#endif

#ifdef NFFS_PRESENT
#define MY_CONFIG_FILE "/cfg/run"

static struct conf_file my_conf = {
    .cf_name = MY_CONFIG_FILE
};

static void
setup_for_nffs(void)
{
    struct nffs_area_desc nffs_descs[NFFS_AREA_MAX + 1];
    int cnt;
    int rc;

    /*
     * Make sure we have enough left to initialize the NFFS with the
     * right number of maximum areas otherwise the file-system will not
     * be readable.
     */
    cnt = NFFS_AREA_MAX;
    rc = flash_area_to_nffs_desc(FLASH_AREA_NFFS, &cnt, nffs_descs);
    assert(rc == 0);

    /*
     * Initializes the flash driver and file system for use by the boot loader.
     */
    rc = nffs_init();
    if (rc == 0) {
        /* Look for an nffs file system in internal flash.  If no file
         * system gets detected, all subsequent file operations will fail,
         * but the boot loader should proceed anyway.
         */
        nffs_detect(nffs_descs);
    }

    rc = conf_file_src(&my_conf);
    assert(rc == 0);
    rc = conf_file_dst(&my_conf);
    assert(rc == 0);
}
#else
struct flash_area conf_fcb_area[NFFS_AREA_MAX + 1];

static struct conf_fcb my_conf = {
    .cf_fcb.f_magic = 0xc09f6e5e,
    .cf_fcb.f_sectors = conf_fcb_area
};

static void
setup_for_fcb(void)
{
    int cnt;
    int rc;

    rc = flash_area_to_sectors(FLASH_AREA_NFFS, &cnt, NULL);
    assert(rc == 0);
    assert(cnt <= sizeof(conf_fcb_area) / sizeof(conf_fcb_area[0]));
    flash_area_to_sectors(FLASH_AREA_NFFS, &cnt, conf_fcb_area);

    my_conf.cf_fcb.f_sector_cnt = cnt;

    conf_fcb_src(&my_conf);
    conf_fcb_dst(&my_conf);
}
#endif

int
main(void)
{
    struct flash_area descs[AREA_DESC_MAX];
    /** Areas representing the beginning of image slots. */
    uint8_t img_starts[2];
    int cnt;
    int total;
    struct boot_rsp rsp;
    int rc;
    struct boot_req req = {
        .br_area_descs = descs,
        .br_slot_areas = img_starts,
    };

    os_init();

    rc = hal_flash_init();
    assert(rc == 0);

    cnt = BOOT_AREA_DESC_MAX;
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_0, &cnt, descs);
    img_starts[0] = 0;
    total = cnt;

    cnt = BOOT_AREA_DESC_MAX - total;
    assert(cnt >= 0);
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_1, &cnt, &descs[total]);
    assert(rc == 0);
    img_starts[1] = total;
    total += cnt;

    cnt = BOOT_AREA_DESC_MAX - total;
    assert(cnt >= 0);
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_SCRATCH, &cnt, &descs[total]);
    assert(rc == 0);
    req.br_scratch_area_idx = total;
    total += cnt;

    req.br_num_image_areas = total;

    conf_init();

#ifdef NFFS_PRESENT
    setup_for_nffs();
#elif FCB_PRESENT
    setup_for_fcb();
#endif
    bootutil_cfg_register();

#ifdef BOOT_SERIAL
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

