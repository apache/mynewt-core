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

#include <string.h>
#include <assert.h>
#include "os/mynewt.h"
#include "modlog/modlog.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "imgmgr/imgmgr.h"
#include "config/config.h"
#include "config/config_file.h"
#include "reboot/log_reboot.h"
#include "bsp/bsp.h"
#include "flash_map/flash_map.h"

#if MYNEWT_VAL(REBOOT_LOG_FCB)
#include "fcb/fcb.h"
#endif

uint16_t reboot_cnt;
static uint16_t soft_reboot;
static char reboot_cnt_str[12];
static char soft_reboot_str[12];
static char *reboot_conf_get(int argc, char **argv, char *buf, int max_len);
static int reboot_conf_set(int argc, char **argv, char *val);
static int reboot_conf_export(void (*export_func)(char *name, char *val),
                             enum conf_export_tgt tgt);

struct conf_handler reboot_conf_handler = {
    .ch_name = "reboot",
    .ch_get = reboot_conf_get,
    .ch_set = reboot_conf_set,
    .ch_commit = NULL,
    .ch_export = reboot_conf_export
};

#if MYNEWT_VAL(REBOOT_LOG_FCB)
static struct fcb_log reboot_log_fcb;
static struct log reboot_log;
static struct flash_area sector;
#endif


#if MYNEWT_VAL(REBOOT_LOG_CONSOLE)
static int
log_reboot_init_console(void)
{
    int rc;

    rc = modlog_register(LOG_MODULE_REBOOT, log_console_get(), LOG_SYSLEVEL,
                         NULL);
    if (rc != 0) {
        return rc;
    }

    return 0;

}
#endif

#if MYNEWT_VAL(REBOOT_LOG_FCB)
static int
log_reboot_init_fcb(void)
{
    const struct flash_area *ptr;
    struct fcb *fcbp;
    int rc;

    if (flash_area_open(MYNEWT_VAL(REBOOT_LOG_FLASH_AREA), &ptr)) {
        return SYS_EUNKNOWN;
    }
    fcbp = &reboot_log_fcb.fl_fcb;
    sector = *ptr;
    fcbp->f_sectors = &sector;
    fcbp->f_sector_cnt = 1;
    fcbp->f_magic = 0x7EADBADF;
    fcbp->f_version = g_log_info.li_version;

    reboot_log_fcb.fl_entries = MYNEWT_VAL(REBOOT_LOG_ENTRY_COUNT);

    rc = fcb_init(fcbp);
    if (rc) {
        flash_area_erase(ptr, 0, ptr->fa_size);
        rc = fcb_init(fcbp);
        if (rc) {
            return rc;
        }
    }

    rc = log_register("reboot_log", &reboot_log, &log_fcb_handler,
                      &reboot_log_fcb, LOG_SYSLEVEL);
    if (rc != 0) {
        return rc;
    }

    rc = modlog_register(LOG_MODULE_REBOOT, &reboot_log, LOG_SYSLEVEL,
                         NULL);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
#endif

static int
reboot_cnt_inc(void)
{
    char str[12];
    int rc;

    reboot_cnt++;
    rc = conf_save_one("reboot/reboot_cnt",
                       conf_str_from_value(CONF_INT16, &reboot_cnt,
                                           str, sizeof(str)));
    return rc;
}

/**
 * Logs reboot with the specified reason
 * @param reason for reboot
 * @return 0 on success; non-zero on failure
 */
int
log_reboot(enum hal_reset_reason reason)
{
    struct image_version ver;

#if MYNEWT_VAL(REBOOT_LOG_FCB)
    {
        const struct flash_area *ptr;
        if (flash_area_open(MYNEWT_VAL(REBOOT_LOG_FLASH_AREA), &ptr)) {
            return 0;
        }
    }
#endif

    if (reason == HAL_RESET_REQUESTED) {
        conf_save_one("reboot/soft_reboot", "1");
    } else {
        conf_save_one("reboot/soft_reboot", "0");
    }

    if (!soft_reboot || reason != HAL_RESET_SOFT) {
        imgr_my_version(&ver);

        /* Log a reboot */
        MODLOG_CRITICAL(LOG_MODULE_REBOOT, "rsn:%s, cnt:%u,"
                        " img:%u.%u.%u.%u", REBOOT_REASON_STR(reason),
                        reboot_cnt, ver.iv_major, ver.iv_minor,
                        ver.iv_revision, (unsigned int)ver.iv_build_num);
    }

    return 0;
}

/**
 * Increments the reboot counter and writes an entry to the reboot log, if
 * necessary.  This function should be called from main() after config
 * settings have been loaded via conf_load().
 *
 * @param reason                The cause of the reboot.
 */
void
reboot_start(enum hal_reset_reason reason)
{
    reboot_cnt_inc();
    log_reboot(reason);
}

static char *
reboot_conf_get(int argc, char **argv, char *buf, int max_len)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "reboot_cnt")) {
            return conf_str_from_value(CONF_INT16, &reboot_cnt,
                                       reboot_cnt_str, sizeof reboot_cnt_str);
        } else if (!strcmp(argv[0], "soft_reboot")) {
            return conf_str_from_value(CONF_INT16, &soft_reboot,
                                       soft_reboot_str,
                                       sizeof soft_reboot_str);
        }
    }
    return NULL;
}

static int
reboot_conf_set(int argc, char **argv, char *val)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "reboot_cnt")) {
            return CONF_VALUE_SET(val, CONF_INT16, reboot_cnt);
        } else if (!strcmp(argv[0], "soft_reboot")) {
            return CONF_VALUE_SET(val, CONF_INT16, soft_reboot);
        }
    }

    return OS_ENOENT;
}

static int
reboot_conf_export(void (*func)(char *name, char *val),
                   enum conf_export_tgt tgt)
{
    if (tgt == CONF_EXPORT_SHOW) {
        func("reboot/reboot_cnt", reboot_cnt_str);
        func("reboot/soft_reboot", soft_reboot_str);
    }
    return 0;
}

void
log_reboot_pkg_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = conf_register(&reboot_conf_handler);
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(REBOOT_LOG_FCB)
    rc = log_reboot_init_fcb();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
#if MYNEWT_VAL(REBOOT_LOG_CONSOLE)
    rc = log_reboot_init_console();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
