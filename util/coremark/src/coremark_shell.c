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

#include <shell/shell.h>
#include <hal/hal_watchdog.h>
#include <coremark/coremark_api.h>

extern uint32_t SystemCoreClock;

static int
coremark_shell_cmd(int argc, char **argv)
{
    printf("Coremark running on %s at %lu MHz\n\n",
           MYNEWT_VAL(BSP_NAME), SystemCoreClock / 1000000L);

    if(MYNEWT_VAL(WATCHDOG_INTERVAL) > 0) {
        hal_watchdog_tickle();
    }
    coremark_run();
    if(MYNEWT_VAL(WATCHDOG_INTERVAL) > 0) {
        hal_watchdog_tickle();
    }

    return 0;
}

static const struct shell_cmd_help coremark_help = {
    .summary = "Run coremark benchmark",
    .params = NULL,
    .usage = NULL,
};

static const struct shell_cmd coremark_shell_cmd_struct = {
    .sc_cmd = "coremark",
    .sc_cmd_func = coremark_shell_cmd,
    .help = &coremark_help,
};

void
coremark_shell_init_pkg(void)
{
#if MYNEWT_VAL(SHELL_COMPAT)
    int rc;
    rc = shell_cmd_register(&coremark_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
