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

#include <os/os.h>
#include <fcb/fcb.h>
#include <console/console.h>
#include "log/log.h"
#include <bootutil/image.h>
#include <bootutil/bootutil_misc.h>
#include <imgmgr/imgmgr.h>
#include <string.h>
#include <config/config.h>
#include <config/config_file.h>
#include <reboot/log_reboot.h>
#include <bsp/bsp.h>
#include <assert.h>

#ifdef SHELL_PRESENT
#include <shell/shell.h>
#endif

static struct log_handler reboot_log_handler;
static struct fcb fcb;
static struct log reboot_log;
static uint16_t reboot_cnt;
static uint16_t soft_reboot;
static char reboot_cnt_str[12];
static char soft_reboot_str[12];
static char *reboot_cnt_get(int argc, char **argv, char *buf, int max_len);
static int reboot_cnt_set(int argc, char **argv, char *val);
static struct flash_area sector;

struct conf_handler reboot_conf_handler = {
    .ch_name = "reboot",
    .ch_get = reboot_cnt_get,
    .ch_set = reboot_cnt_set,
    .ch_commit = NULL,
    .ch_export = NULL
};

/**
 * Reboot log initilization
 * @param type of log(console or storage); number of entries to restore
 * @return 0 on success; non-zero on failure
 */
int
reboot_init_handler(int log_type, uint8_t entries)
{
    int rc;
    const struct flash_area *ptr;

    rc = conf_register(&reboot_conf_handler);

    switch (log_type) {
        case LOG_TYPE_STORAGE:
            if (flash_area_open(FLASH_AREA_REBOOT_LOG, &ptr)) {
                goto err;
            }
            sector = *ptr;
            fcb.f_sectors = &sector;
            fcb.f_sector_cnt = 1;
            fcb.f_magic = 0x7EADBADF;
            fcb.f_version = 0;

            rc = fcb_init(&fcb);
            if (rc) {
                goto err;
            }
            rc = log_fcb_handler_init(&reboot_log_handler, &fcb, entries);
            if (rc) {
                goto err;
            }
            break;
       case LOG_TYPE_STREAM:
            rc = log_console_handler_init(&reboot_log_handler);
            if (rc) {
                goto err;
            }
            break;
       default:
            assert(0);
    }

    rc = log_register("reboot_log", &reboot_log, &reboot_log_handler);
err:
    return (rc);
}

/**
 * Logs reboot with the specified reason
 * @param reason for reboot
 * @return 0 on success; non-zero on failure
 */
int
log_reboot(int reason)
{
    int rc;
    char str[12] = {0};
    struct image_version ver;
    int16_t reboot_tmp_cnt;
    const struct flash_area *ptr;

    rc = 0;
    if (flash_area_open(FLASH_AREA_REBOOT_LOG, &ptr)) {
        goto err;
    }

    reboot_tmp_cnt = reboot_cnt;

    if (reason == SOFT_REBOOT) {
        /*
         * Save reboot count as soft reboot cnt if the reason is
         * a soft reboot
         */
        reboot_tmp_cnt = reboot_cnt + 1;
        conf_save_one("reboot/soft_reboot",
                      conf_str_from_value(CONF_INT16, &reboot_tmp_cnt,
                                          str, sizeof(str)));
    } else if (reason == HARD_REBOOT) {
        conf_save_one("reboot/soft_reboot", "0");
        if (soft_reboot) {
            /* No need to log as it's not a hard reboot */
            goto err;
        } else {
            reboot_cnt++;
            reboot_tmp_cnt = reboot_cnt;
        }
    }

    /*
     * Only care for this return code as it will tell whether the config is
     * full, the caller of the function might not care about the return code
     * Saving the reboot cnt
     */
    rc = conf_save_one("reboot/reboot_cnt",
                       conf_str_from_value(CONF_INT16, &reboot_tmp_cnt,
                                           str, sizeof(str)));

    imgr_my_version(&ver);

    /* Log a reboot */
    LOG_CRITICAL(&reboot_log, LOG_MODULE_REBOOT, "rsn:%s, cnt:%u,"
                 " img:%u.%u.%u.%u", REBOOT_REASON_STR(reason), reboot_tmp_cnt,
                 ver.iv_major, ver.iv_minor, ver.iv_revision,
                 (unsigned int)ver.iv_build_num);
err:
    return (rc);
}

static char *
reboot_cnt_get(int argc, char **argv, char *buf, int max_len)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "reboot_cnt")) {
            return reboot_cnt_str;
        } else if (!strcmp(argv[0], "soft_reboot")) {
            return soft_reboot_str;
        }
    }
    return NULL;
}

static int
reboot_cnt_set(int argc, char **argv, char *val)
{
    int rc;

    if (argc == 1) {
        if (!strcmp(argv[0], "reboot_cnt")) {
            rc = CONF_VALUE_SET(val, CONF_STRING, reboot_cnt_str);
            if (rc) {
                goto err;
            }

            return CONF_VALUE_SET(reboot_cnt_str, CONF_INT16, reboot_cnt);
        } else if (!strcmp(argv[0], "soft_reboot")) {
            rc = CONF_VALUE_SET(val, CONF_STRING, soft_reboot_str);
            if (rc) {
                goto err;
            }

            return CONF_VALUE_SET(soft_reboot_str, CONF_INT16, soft_reboot);
        }
    }

err:
    return OS_ENOENT;
}
