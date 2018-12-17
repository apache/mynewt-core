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
#include "lps33thw/lps33thw.h"
#include "lps33thw_priv.h"

#if MYNEWT_VAL(LPS33THW_CLI)

#include "shell/shell.h"
#include "parse/parse.h"

static int lps33thw_shell_cmd(int argc, char **argv);

static struct shell_cmd lps33thw_shell_cmd_struct = {
    .sc_cmd = "lps33thw",
    .sc_cmd_func = lps33thw_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    .si_dev = NULL, /* will be updated on 1st access */
#else
    .si_type = MYNEWT_VAL(LPS33THW_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(LPS33THW_SHELL_ITF_NUM),
    .si_addr = MYNEWT_VAL(LPS33THW_SHELL_ITF_ADDR)
#endif
};

static int
lps33thw_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lps33thw_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lps33thw_shell_cmd_read_press(int argc, char **argv)
{
    int rc;
    float press;
    char tmpstr[13];

    if (argc > 2) {
        return lps33thw_shell_err_too_many_args(argv[1]);
    }

    rc = lps33thw_get_pressure(&g_sensor_itf, &press);
    if (rc) {
        console_printf("Read falied: %d\r\n", rc);
        return rc;
    }

    console_printf("Pressure: %s\r\n", sensor_ftostr(press, tmpstr, 13));
    return 0;
}

static int
lps33thw_shell_cmd_read_temp(int argc, char **argv)
{
    int rc;
    float temp;
    char tmpstr[13];

    if (argc > 2) {
        return lps33thw_shell_err_too_many_args(argv[1]);
    }

    rc = lps33thw_get_temperature(&g_sensor_itf, &temp);
    if (rc) {
        console_printf("Read falied: %d\r\n", rc);
        return rc;
    }

    console_printf("Temperature: %s\r\n", sensor_ftostr(temp, tmpstr, 13));
    return 0;
}

static int
lps33thw_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", lps33thw_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\trp\n");
    console_printf("\trt\n");

    return 0;
}

static int
lps33thw_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return lps33thw_shell_help();
    }

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    if (!g_sensor_itf.si_dev) {
        g_sensor_itf.si_dev = os_dev_open(MYNEWT_VAL(LPS33THW_SHELL_NODE_NAME),
                                          0, NULL);

        if (!g_sensor_itf.si_dev) {
            console_printf("Cannot open device '%s'\n",
                           MYNEWT_VAL(LPS33THW_SHELL_NODE_NAME));
            return 0;
        }
    }
#endif

    /* Read pressure */
    if (argc > 1 && strcmp(argv[1], "rp") == 0) {
        return lps33thw_shell_cmd_read_press(argc, argv);
    }

    /* Read temperature */
    if (argc > 1 && strcmp(argv[1], "rt") == 0) {
        return lps33thw_shell_cmd_read_temp(argc, argv);
    }

    return lps33thw_shell_err_unknown_arg(argv[1]);
}

int
lps33thw_shell_init(void)
{
    int rc;
    rc = shell_cmd_register(&lps33thw_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif




