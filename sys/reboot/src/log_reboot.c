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
#include <util/cbmem.h>
#include <console/console.h>
#include "log/log.h"
#include <bootutil/image.h>
#include <bootutil/bootutil_misc.h>
#include <imgmgr/imgmgr.h>
#include <string.h>
#include <config/config.h>
#include <config/config_file.h>
#include <reboot/log_reboot.h>
#ifdef SHELL_PRESENT
#include <shell/shell.h>
#endif

#define MAX_CBMEM_BUF 300

static struct flash_area fcb_areas[] = {
    [0] = {
        .fa_off = 0x00018000,
        .fa_size = (2 * 1024)
    }
};

static struct log_handler reboot_log_handler;
static struct fcb log_fcb;
static struct log reboot_log;
static struct cbmem reboot_cbmem;
static uint32_t reboot_cbmem_buf[MAX_CBMEM_BUF];
static uint16_t reboot_cnt;
static uint16_t soft_reboot;
static char reboot_cnt_str[32];
static char soft_reboot_str[32];
static char *reboot_cnt_get(int argc, char **argv, char *buf, int max_len);
static int reboot_cnt_set(int argc, char **argv, char *val);

struct conf_handler reboot_conf_handler = {
    .ch_name = "reboot",
    .ch_get = reboot_cnt_get,
    .ch_set = reboot_cnt_set,
    .ch_commit = NULL,
    .ch_export = NULL
};

/**
 * Reboot log initilization
 */
int
reboot_init_handler(int log_type)
{
    int rc;

    conf_register(&reboot_conf_handler);

    switch (log_type) {
        case LOG_TYPE_STORAGE:
            log_fcb.f_sectors = fcb_areas;
            log_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);
            log_fcb.f_magic = 0x7EADBADF;
            log_fcb.f_version = 0;
            rc = fcb_init(&log_fcb);
            rc = log_fcb_handler_init(&reboot_log_handler, &log_fcb);

            break;
       case LOG_TYPE_STREAM:
            (void) console_init(NULL);

            break;
       case LOG_TYPE_MEMORY:
       default:
            cbmem_init(&reboot_cbmem, reboot_cbmem_buf, MAX_CBMEM_BUF);
            log_cbmem_handler_init(&reboot_log_handler, &reboot_cbmem);
    }

    imgmgr_module_init();
    rc = log_register("reboot_log", &reboot_log, &reboot_log_handler);
    return (rc);
}

int
log_reboot(int reason)
{
    int rc;
    char str[12] = {0};
    struct image_version ver;

    rc = 0;

    reboot_cnt++;

    if (reason != SOFT_REBOOT) {
        /* If number of reboots are not equal to soft reboots */
        if ((reboot_cnt - 1) != soft_reboot) {
            reason = HARD_REBOOT;
        } else {
            /* If the count is equal */
            goto err;
        }
    } else {
        /*
         * Save reboot count as soft reboot cnt if the reason is
         * a hard reboot
         */
        rc = conf_save_one(&reboot_conf_handler, "soft_reboot",
                           conf_str_from_value(CONF_INT16, &reboot_cnt,
                                               str, sizeof(str)));
    }

    imgr_my_version(&ver);

    /* Save the reboot cnt */
    rc = conf_save_one(&reboot_conf_handler, "reboot_cnt",
                       conf_str_from_value(CONF_INT16, &reboot_cnt,
                                           str, sizeof(str)));
    if (rc) {
        goto err;
    }

    /* Log a reboot */
    LOG_CRITICAL(&reboot_log, LOG_MODULE_REBOOT, "rsn:%s, cnt:%u,"
                 " img:%u.%u.%u.%u", REBOOT_REASON_STR(reason), reboot_cnt,
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
