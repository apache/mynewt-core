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

#include <inttypes.h>
#include "os/mynewt.h"
#include <shell/shell.h>
#include <console/console.h>
#include <parse/parse.h>
#include <bsp/bsp.h>
#include <errno.h>
#include <modlog/modlog.h>

static os_stack_t spam_task_stack[MYNEWT_VAL(CONSOLE_SPAM_TASK_STACK_SIZE)] __attribute__ ((aligned (8)));
static struct os_task spam_task;
static bool spam;
static int spam_interval;

static int spam_cli_cmd(int argc, char **argv);

static struct shell_cmd spam_cmd_struct = {
    .sc_cmd = "spam",
    .sc_cmd_func = spam_cli_cmd
};

static int
spam_cli_cmd(int argc, char **argv)
{
    if (argc < 2) {
        return 0;
    }
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0) {
        console_printf("usage:\n");
        console_printf("   %s on\n", argv[0]);
        console_printf("   %s off:\n", argv[0]);
        console_printf("   %s <interval_in_ms>\n", argv[0]);
    } else if (strcmp(argv[1], "on") == 0) {
        spam = true;
    } else if (strcmp(argv[1], "off") == 0) {
        spam = false;
    } else if (strcmp(argv[1], "spam") == 0) {
        if (argc > 2) {
            spam_interval = atoi(argv[2]);
            if (spam_interval < 2 * 1000 / OS_TICKS_PER_SEC) {
                spam_interval = 2 * 1000 / OS_TICKS_PER_SEC;
            }
            spam_interval = os_time_ms_to_ticks32(spam_interval);
        }
        spam = true;
    } else {
        spam_interval = atoi(argv[1]);
        if (spam_interval < 2 * 1000 / OS_TICKS_PER_SEC) {
            spam_interval = 2 * 1000 / OS_TICKS_PER_SEC;
        }
        spam_interval = os_time_ms_to_ticks32(spam_interval);
        spam = true;
    }
    return 0;
}

static void
spam_task_f(void *arg)
{
    volatile int i = 0;

    spam_interval = os_time_ms_to_ticks32(MYNEWT_VAL(CONSOLE_SPAM_INTERVAL));

    /* Main log handling loop */
    while (1)
    {
        if (spam) {
            console_printf("Just spamming console %d\n", i++);
            console_printf("Lets add some longer lines to check if it matters\n");
            console_printf("Even more spamming text sent at once\n");
            MODLOG_DFLT(DEBUG, "spamming debug log\n");
            MODLOG_DFLT(INFO, "spamming error log\n");
        }
        os_time_delay(spam_interval);
    }
}

void
console_spam_init(void)
{
    int rc;

    if (MYNEWT_VAL(CONSOLE_SPAM_TASK_ENABLE)) {
        rc = os_task_init(&spam_task, "spam", spam_task_f, NULL,
                          MYNEWT_VAL(CONSOLE_SPAM_TASK_PRIORITY), OS_WAIT_FOREVER,
                          spam_task_stack,
                          MYNEWT_VAL(CONSOLE_SPAM_TASK_STACK_SIZE));
        assert(rc == 0);
        shell_cmd_register(&spam_cmd_struct);
    }
}
