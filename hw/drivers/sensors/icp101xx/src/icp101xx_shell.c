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
#include <stdio.h>
#include "os/mynewt.h"
#include "console/console.h"
#include "shell/shell.h"
#include "hal/hal_gpio.h"
#include "icp101xx_priv.h"
#include "icp101xx/icp101xx.h"
#include "sensor/sensor.h"
#include "sensor/pressure.h"
#include "hal/hal_i2c.h"
#include "parse/parse.h"

#if MYNEWT_VAL(ICP101XX_CLI)

static int icp101xx_shell_cmd(int argc, char **argv);

static struct shell_cmd icp101xx_shell_cmd_struct = {
    .sc_cmd = "icp101xx",
    .sc_cmd_func = icp101xx_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(ICP101XX_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(ICP101XX_SHELL_ITF_NUM),
    .si_addr = MYNEWT_VAL(ICP101XX_SHELL_ITF_ADDR),
};

static int
icp101xx_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
icp101xx_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
icp101xx_shell_help(void)
{
    console_printf("%s cmd  [flags...]\n", icp101xx_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tchip_id\n");
    console_printf("\tsoft_reset\n");

    return 0;
}

static int
icp101xx_shell_cmd_get_chip_id(int argc, char **argv)
{
    uint16_t id;
    uint8_t data_read[2] = {0};
    int rc;

    if (argc > 3) {
        return icp101xx_shell_err_too_many_args(argv[1]);
    }

    /* Display the chip id */
    if (argc == 2) {
        /* Read ID of pressure sensor */
        rc = icp101xx_read_reg(&g_sensor_itf, ICP101XX_CMD_READ_ID, data_read, 2);
        if (rc) {
            console_printf("Read failed %d", rc);
            return rc;
        }
        id = data_read[0] << 8 | data_read[1];

        console_printf("Read ID register : 0x%x\n", id);
    }

    return 0;
}

static int
icp101xx_shell_cmd_soft_reset(int argc, char **argv)
{
    return icp101xx_write_reg(&g_sensor_itf, ICP101XX_CMD_SOFT_RESET, NULL, 0);
}

static int
icp101xx_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return icp101xx_shell_help();
    }

    /* Chip ID command */
    if (argc > 1 && strcmp(argv[1], "chip_id") == 0) {
        return icp101xx_shell_cmd_get_chip_id(argc, argv);
    }

    /* Soft Reset command*/
    if (argc > 1 && strcmp(argv[1], "soft_reset") == 0) {
        return icp101xx_shell_cmd_soft_reset(argc, argv);
    }

    return icp101xx_shell_err_unknown_arg(argv[1]);
}

int
icp101xx_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&icp101xx_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
