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

#include "shell/shell.h"

#include "memfault/demo/cli.h"
#include "memfault/metrics/metrics.h"

#include "memfault_common.h"

static int
prv_clear_core_cmd(int argc, char **argv)
{
    return memfault_demo_cli_cmd_clear_core(argc, argv);
}

static int
prv_get_core_cmd(int argc, char **argv)
{
    return memfault_demo_cli_cmd_get_core(argc, argv);
}

static int
prv_crash_example(int argc, char **argv)
{
    return memfault_demo_cli_cmd_crash(argc, argv);
}

static int
prv_get_device_info(int argc, char **argv)
{
    return memfault_demo_cli_cmd_get_device_info(argc, argv);
}

static int
prv_print_chunk_cmd(int argc, char **argv)
{
    return memfault_demo_cli_cmd_print_chunk(argc, argv);
}

static int
prv_heartbeat_trigger(int argc, char **argv)
{
    memfault_metrics_heartbeat_debug_trigger();
    return 0;
}

static int
prv_heartbeat_print(int argc, char **argv)
{
    memfault_metrics_heartbeat_debug_print();
    return 0;
}

static const struct shell_cmd os_commands[] = {
    SHELL_CMD("crash", prv_crash_example, NULL),
    SHELL_CMD("clear_core", prv_clear_core_cmd, NULL),
    SHELL_CMD("get_core", prv_get_core_cmd, NULL),
    SHELL_CMD("get_device_info", prv_get_device_info, NULL),
    SHELL_CMD("print_chunk", prv_print_chunk_cmd, NULL),
    SHELL_CMD("heartbeat_trigger", prv_heartbeat_trigger, NULL),
    SHELL_CMD("heartbeat_print", prv_heartbeat_print, NULL),
    {0},
};

void
shell_mflt_register(void)
{
    int rc;

#if !MYNEWT_VAL(SYS_MEMFAULT_CLI)
    return;
#endif

    rc = shell_register("mflt", os_commands);
    SYSINIT_PANIC_ASSERT_MSG(
        rc == 0, "Failed to register OS shell commands");

    shell_register_default_module("mflt");
}
