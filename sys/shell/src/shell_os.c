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

#include "shell/shell.h"
#include "shell_priv.h"

#define SHELL_OS "os"

int
shell_os_tasks_display_cmd(int argc, char **argv)
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

    console_printf("Tasks: \n");
    prev_task = NULL;
    console_printf("%8s %3s %3s %8s %8s %8s %8s %8s %8s %3s\n",
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

        console_printf("%8s %3u %3u %8lu %8lu %8u %8u %8lu %8lu\n",
                oti.oti_name, oti.oti_prio, oti.oti_taskid,
                (unsigned long)oti.oti_runtime, (unsigned long)oti.oti_cswcnt,
                oti.oti_stksize, oti.oti_stkusage,
                (unsigned long)oti.oti_last_checkin,
                (unsigned long)oti.oti_next_checkin);

    }

    if (name && !found) {
        console_printf("Couldn't find task with name %s\n", name);
    }

    return 0;
}

int
shell_os_mpool_display_cmd(int argc, char **argv)
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

    console_printf("Mempools: \n");
    mp = NULL;
    console_printf("%32s %5s %4s %4s %4s\n", "name", "blksz", "cnt", "free",
                   "min");
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

        console_printf("%32s %5d %4d %4d %4d\n", omi.omi_name,
                       omi.omi_block_size, omi.omi_num_blocks,
                       omi.omi_num_free, omi.omi_min_free);
    }

    if (name && !found) {
        console_printf("Couldn't find a memory pool with name %s\n",
                name);
    }

    return 0;
}

int
shell_os_date_cmd(int argc, char **argv)
{
    struct os_timeval tv;
    struct os_timezone tz;
    char buf[DATETIME_BUFSIZE];
    int rc;

    argc--; argv++;     /* skip command name */

    if (argc == 0) {
        /* Display the current datetime */
        rc = os_gettimeofday(&tv, &tz);
        assert(rc == 0);
        rc = datetime_format(&tv, &tz, buf, sizeof(buf));
        assert(rc == 0);
        console_printf("%s\n", buf);
    } else if (argc == 1) {
        /* Set the current datetime */
        rc = datetime_parse(*argv, &tv, &tz);
        if (rc == 0) {
            rc = os_settimeofday(&tv, &tz);
        } else {
            console_printf("Invalid datetime\n");
        }
    } else {
        rc = -1;
    }

    return rc;
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

static const struct shell_param date_params[] = {
    {"", "datetime to set"},
    {NULL, NULL}
};

static const struct shell_cmd_help date_help = {
    .summary = "show system date",
    .usage = NULL,
    .params = date_params,
};
#endif

static const struct shell_cmd os_commands[] = {
    {
        .sc_cmd = "tasks",
        .sc_cmd_func = shell_os_tasks_display_cmd,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &tasks_help,
#endif
    },
    {
        .sc_cmd = "mpool",
        .sc_cmd_func = shell_os_mpool_display_cmd,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &mpool_help,
#endif
    },
    {
        .sc_cmd = "date",
        .sc_cmd_func = shell_os_date_cmd,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &date_help,
#endif
    },
    { NULL, NULL, NULL },
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
