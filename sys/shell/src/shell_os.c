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
#include <string.h>

#include "os/mynewt.h"
#include "datetime/datetime.h"
#include "console/console.h"

#include "hal/hal_system.h"
#include "hal/hal_nvreg.h"
#include "streamer/streamer.h"
#include "shell/shell.h"
#include "shell_priv.h"

#define SHELL_OS "os"

static int
shell_os_tasks_display_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                           struct streamer *streamer)
{
    struct os_task *prev_task;
    struct os_task_info oti;
    char *name;
    int found;

    name = NULL;
    found = 0;

    if (argc > 1 && strcmp(argv[1], "")) {
        name = argv[1];
    }

    streamer_printf(streamer, "Tasks: \n");
    prev_task = NULL;
    streamer_printf(streamer, "%8s %3s %3s %8s %8s %8s %8s %8s %8s %3s\n",
      "task", "pri", "tid", "runtime", "csw", "stksz", "stkuse",
      "lcheck", "ncheck", "flg");
    while (1) {
        prev_task = os_task_info_get_next(prev_task, &oti);
        if (prev_task == NULL) {
            break;
        }

        if (name) {
            if (strcmp(name, oti.oti_name)) {
                continue;
            } else {
                found = 1;
            }
        }

        streamer_printf(streamer, "%8s %3u %3u %8lu %8lu %8u %8u %8lu %8lu\n",
                oti.oti_name, oti.oti_prio, oti.oti_taskid,
                (unsigned long)oti.oti_runtime, (unsigned long)oti.oti_cswcnt,
                oti.oti_stksize, oti.oti_stkusage,
                (unsigned long)oti.oti_last_checkin,
                (unsigned long)oti.oti_next_checkin);

    }

    if (name && !found) {
        streamer_printf(streamer, "Couldn't find task with name %s\n", name);
    }

    return 0;
}

int
shell_os_mpool_display_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                           struct streamer *streamer)
{
    struct os_mempool *mp;
    struct os_mempool_info omi;
    char *name;
    int found;

    name = NULL;
    found = 0;

    if (argc > 1 && strcmp(argv[1], "")) {
        name = argv[1];
    }

    streamer_printf(streamer, "Mempools: \n");
    mp = NULL;
    streamer_printf(streamer, "%32s %5s %4s %4s %4s\n",
                    "name", "blksz", "cnt", "free", "min");
    while (1) {
        mp = os_mempool_info_get_next(mp, &omi);
        if (mp == NULL) {
            break;
        }

        if (name) {
            if (strcmp(name, omi.omi_name)) {
                continue;
            } else {
                found = 1;
            }
        }

        streamer_printf(streamer, "%32s %5d %4d %4d %4d\n", omi.omi_name,
                       omi.omi_block_size, omi.omi_num_blocks,
                       omi.omi_num_free, omi.omi_min_free);
    }

    if (name && !found) {
        streamer_printf(streamer, "Couldn't find a memory pool with name %s\n",
                name);
    }

    return 0;
}

int
shell_os_date_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                  struct streamer *streamer)
{
    int rc = 0;
#if MYNEWT_VAL(SHELL_OS_DATETIME_CMD)
    struct os_timeval tv;
    struct os_timezone tz;
    char buf[DATETIME_BUFSIZE];

    argc--; argv++;     /* skip command name */
    rc = -1;

    if (argc == 0) {
#if (MYNEWT_VAL(SHELL_OS_DATETIME_CMD) & 1) == 1
        /* Display the current datetime */
        rc = os_gettimeofday(&tv, &tz);
        assert(rc == 0);
        rc = datetime_format(&tv, &tz, buf, sizeof(buf));
        assert(rc == 0);
        streamer_printf(streamer, "%s\n", buf);
#endif
    } else if (argc == 1) {
#if (MYNEWT_VAL(SHELL_OS_DATETIME_CMD) & 2) == 2
        /* Set the current datetime */
        rc = datetime_parse(*argv, &tv, &tz);
        if (rc == 0) {
            rc = os_settimeofday(&tv, &tz);
        } else {
            streamer_printf(streamer, "Invalid datetime\n");
        }
#endif
    } else {
        rc = -1;
    }
#endif
    return rc;
}

int
shell_os_reset_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                   struct streamer *streamer)
{
#if MYNEWT_VAL(SHELL_OS_SERIAL_BOOT_NVREG)
    if (argc == 2 && !strcmp(argv[1], "serial_boot")) {
        hal_nvreg_write(MYNEWT_VAL(BOOT_SERIAL_NVREG_INDEX),
                        MYNEWT_VAL(BOOT_SERIAL_NVREG_MAGIC));
        streamer_printf(streamer, "serial_boot mode\n");
    }
#endif
    os_time_delay(OS_TICKS_PER_SEC / 10);
    os_reboot(HAL_RESET_REQUESTED);
    return 0;
}

static int
shell_os_ls_dev(struct os_dev *dev, void *arg)
{
    struct streamer *streamer;

    streamer = arg;
    streamer_printf(streamer, "%4d %3x %s\n",
                    dev->od_open_ref, dev->od_flags, dev->od_name);
    return 0;
}

int
shell_os_ls_dev_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                    struct streamer *streamer)
{
    streamer_printf(streamer, "%4s %3s %s\n", "ref", "flg", "name");
    os_dev_walk(shell_os_ls_dev, streamer);
    return 0;
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_param tasks_params[] = {
    {"", "task name"},
    {NULL, NULL}
};

static const struct shell_cmd_help tasks_help = {
    .summary = "show os tasks",
    .usage = NULL,
    .params = tasks_params,
};

static const struct shell_param mpool_params[] = {
    {"", "mpool name"},
    {NULL, NULL}
};

static const struct shell_cmd_help mpool_help = {
    .summary = "show system mpool",
    .usage = NULL,
    .params = mpool_params,
};

#if (MYNEWT_VAL(SHELL_OS_DATETIME_CMD) & 2) == 2
static const struct shell_param date_params[] = {
    {"", "datetime to set"},
    {NULL, NULL}
};
#endif

static const struct shell_cmd_help date_help = {
    .summary = "show system date",
    .usage = NULL,
#if (MYNEWT_VAL(SHELL_OS_DATETIME_CMD) & 2) == 2
    .params = date_params,
#endif
};

static const struct shell_param reset_params[] = {
#if MYNEWT_VAL(SHELL_OS_SERIAL_BOOT_NVREG)
    {"serial_boot", "NVREG write to request serial bootloader entry"},
#endif
    {NULL, NULL}
};

static const struct shell_cmd_help reset_help = {
    .summary = "reset system",
    .usage = NULL,
    .params = reset_params,
};

static const struct shell_cmd_help ls_dev_help = {
    .summary = "list OS devices"
};
#endif

static const struct shell_cmd os_commands[] = {
    SHELL_CMD_EXT("tasks", shell_os_tasks_display_cmd, &tasks_help),
    SHELL_CMD_EXT("mpool", shell_os_mpool_display_cmd, &mpool_help),
    SHELL_CMD_EXT("date", shell_os_date_cmd, &date_help),
    SHELL_CMD_EXT("reset", shell_os_reset_cmd, &reset_help),
    SHELL_CMD_EXT("lsdev", shell_os_ls_dev_cmd, &ls_dev_help),
    { 0 },
};

void
shell_os_register(void)
{
    const struct shell_cmd *cmd;
    int rc;

    for (cmd = os_commands; cmd->sc_cmd != NULL; cmd++) {
        rc = shell_cmd_register(cmd);
        SYSINIT_PANIC_ASSERT_MSG(
            rc == 0, "Failed to register OS shell commands");
    }
}
