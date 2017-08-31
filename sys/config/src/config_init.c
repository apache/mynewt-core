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

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "bsp/bsp.h"

#include "config/config.h"
#include "config/config_file.h"

#if MYNEWT_VAL(CONFIG_NFFS)
#include "fs/fs.h"

static struct conf_file config_init_conf_file = {
    .cf_name = MYNEWT_VAL(CONFIG_NFFS_FILE),
    .cf_maxlines = MYNEWT_VAL(CONFIG_NFFS_MAX_LINES)
};

static void
config_init_fs(void)
{
    int rc;

    fs_mkdir(MYNEWT_VAL(CONFIG_NFFS_DIR));
    rc = conf_file_src(&config_init_conf_file);
    SYSINIT_PANIC_ASSERT(rc == 0);
    rc = conf_file_dst(&config_init_conf_file);
    SYSINIT_PANIC_ASSERT(rc == 0);
}

#elif MYNEWT_VAL(CONFIG_FCB)
#include "fcb/fcb.h"
#include "config/config_fcb.h"

static struct flash_area conf_fcb_area[MYNEWT_VAL(CONFIG_FCB_NUM_AREAS) + 1];

static struct conf_fcb config_init_conf_fcb = {
    .cf_fcb.f_magic = MYNEWT_VAL(CONFIG_FCB_MAGIC),
    .cf_fcb.f_sectors = conf_fcb_area,
};

static void
config_init_fcb(void)
{
    int cnt;
    int rc;

    rc = flash_area_to_sectors(MYNEWT_VAL(CONFIG_FCB_FLASH_AREA), &cnt, NULL);
    SYSINIT_PANIC_ASSERT(rc == 0);
    SYSINIT_PANIC_ASSERT(
        cnt <= sizeof(conf_fcb_area) / sizeof(conf_fcb_area[0]));
    flash_area_to_sectors(
        MYNEWT_VAL(CONFIG_FCB_FLASH_AREA), &cnt, conf_fcb_area);

    config_init_conf_fcb.cf_fcb.f_sector_cnt = cnt;

    rc = conf_fcb_src(&config_init_conf_fcb);
    if (rc) {
        for (cnt = 0;
             cnt < config_init_conf_fcb.cf_fcb.f_sector_cnt;
             cnt++) {

            flash_area_erase(&conf_fcb_area[cnt], 0,
                             conf_fcb_area[cnt].fa_size);
        }
        rc = conf_fcb_src(&config_init_conf_fcb);
    }
    SYSINIT_PANIC_ASSERT(rc == 0);
    rc = conf_fcb_dst(&config_init_conf_fcb);
    SYSINIT_PANIC_ASSERT(rc == 0);
}

#endif

void
config_pkg_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    conf_init();

#if MYNEWT_VAL(CONFIG_NFFS)
    config_init_fs();
#elif MYNEWT_VAL(CONFIG_FCB)
    config_init_fcb();
#endif
}
