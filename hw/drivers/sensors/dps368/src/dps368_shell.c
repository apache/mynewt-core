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
#include <errno.h>
#include "os/mynewt.h"
#include "console/console.h"
#include "hal/hal_gpio.h"
#include "dps368/dps368.h"
#include "dps368_priv.h"

#if MYNEWT_VAL(DPS368_CLI)

#include "shell/shell.h"
#include "parse/parse.h"

static int dps368_shell_cmd(int argc, char **argv);

static struct shell_cmd dps368_shell_cmd_struct = {
    .sc_cmd = "dps368",
    .sc_cmd_func = dps368_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(DPS368_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(DPS368_SHELL_ITF_NUM),
    .si_addr = MYNEWT_VAL(DPS368_SHELL_ITF_ADDR)
};

static int
dps368_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
dps368_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}



static int
dps368_shell_cmd_read_hw_id(int argc, char **argv)
{
    int rc;
    uint8_t id;

    if (argc > 2) {
        return dps368_shell_err_too_many_args(argv[1]);
    }

    rc = dps368_get_hwid(&g_sensor_itf, &id);
    if (rc) {
        console_printf("Read falied: %d\r\n", rc);
        return rc;
    }

    console_printf("HWID: %x\r\n", id);
    return 0;
}

static int
dps368_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", dps368_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tchipid\n");

    return 0;
}

static int
dps368_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return dps368_shell_help();
    }

    /* Read Chip Id */
    if (argc > 1 && strcmp(argv[1], "chipid") == 0) {
        return dps368_shell_cmd_read_hw_id(argc, argv);
    }


    return dps368_shell_err_unknown_arg(argv[1]);
}

int
dps368_shell_init(void)
{
    int rc;
    rc = shell_cmd_register(&dps368_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
