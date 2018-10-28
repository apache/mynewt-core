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
#include "shell/shell.h"
#include "hal/hal_gpio.h"
#include "tsl2591/tsl2591.h"
#include "tsl2591_priv.h"
#include "parse/parse.h"

#if MYNEWT_VAL(TSL2591_CLI)

static int tsl2591_shell_cmd(int argc, char **argv);

static struct shell_cmd tsl2591_shell_cmd_struct = {
    .sc_cmd = "tsl2591",
    .sc_cmd_func = tsl2591_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(TSL2591_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(TSL2591_SHELL_ITF_NUM),
    .si_addr = MYNEWT_VAL(TSL2591_SHELL_ITF_ADDR),
};

static int
tsl2591_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
tsl2591_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
tsl2591_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
tsl2591_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", tsl2591_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tr    [n_samples]\n");
    console_printf("\tgain [0|1|2|3]\n");
    console_printf("\ttime [100|200|300|400|500|600]\n");
    console_printf("\ten   [0|1]\n");
    console_printf("\tdump\n");

    return 0;
}

static int
tsl2591_shell_cmd_read(int argc, char **argv)
{
    uint16_t full;
    uint16_t ir;
    uint16_t samples;
    uint16_t val;
    float lux;
    int rc;

    samples = 1;

    if (argc > 3) {
        return tsl2591_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return tsl2591_shell_err_invalid_arg(argv[2]);
        }
        samples = val;
    }

    while(samples--) {
        /* Get raw broadband and IR readings */
        rc = tsl2591_get_data(&g_sensor_itf, &full, &ir);
        if (rc) {
            console_printf("Read failed: %d\n", rc);
            return rc;
        }
        /* Get lux estimation */
        lux = tsl2591_calculate_lux_f(&g_sensor_itf, full, ir, NULL);
        console_printf("Lux:   %lu.%lu\n", (uint32_t)lux,
                       (uint32_t)(lux*1000)%1000);
        console_printf("Full:  %u\n", full);
        console_printf("IR:    %u\n", ir);
    }

    return 0;
}

static int
tsl2591_shell_cmd_gain(int argc, char **argv)
{
    uint8_t val;
    uint8_t gain;
    int rc;

    if (argc > 3) {
        return tsl2591_shell_err_too_many_args(argv[1]);
    }

    /* Display the gain */
    if (argc == 2) {
        rc = tsl2591_get_gain(&g_sensor_itf, &gain);
        if (rc) {
            console_printf("Getting gain failed rc:%d", rc);
            goto err;
        }
        switch (gain) {
            case TSL2591_LIGHT_GAIN_LOW:
                console_printf("0 (1x)\n");
            break;
            case TSL2591_LIGHT_GAIN_MED:
                console_printf("1 (25x)\n");
            break;
            case TSL2591_LIGHT_GAIN_HIGH:
                console_printf("2 (428x)\n");
            break;
            case TSL2591_LIGHT_GAIN_MAX:
                console_printf("3 (9876x)\n");
            break;
            default:
                console_printf("ERROR!\n");
            break;
        }
    }

    /* Update the gain */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, 3, &rc);
        /* Make sure gain is valid */
        if (rc) {
            return tsl2591_shell_err_invalid_arg(argv[2]);
        }
        rc = tsl2591_set_gain(&g_sensor_itf, val << 4);
        if (rc) {
            console_printf("Setting gain failed rc:%d", rc);
        }
    }

err:
    return rc;
}

static int
tsl2591_shell_cmd_time(int argc, char **argv)
{
    uint8_t time;
    long val;
    int rc;

    if (argc > 3) {
        return tsl2591_shell_err_too_many_args(argv[1]);
    }

    /* Display the integration time */
    if (argc == 2) {
        rc = tsl2591_get_integration_time(&g_sensor_itf, &time);
        if (rc) {
            console_printf("Getting integration time failed rc:%d", rc);
            goto err;
        }

        switch (time) {
            case TSL2591_LIGHT_ITIME_100MS:
                console_printf("100\n");
            break;
            case TSL2591_LIGHT_ITIME_200MS:
                console_printf("200\n");
            break;
            case TSL2591_LIGHT_ITIME_300MS:
                console_printf("300\n");
            break;
            case TSL2591_LIGHT_ITIME_400MS:
                console_printf("400\n");
            break;
            case TSL2591_LIGHT_ITIME_500MS:
                console_printf("500\n");
            break;
            case TSL2591_LIGHT_ITIME_600MS:
                console_printf("600\n");
            break;
        }
    }

    /* Set the integration time */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 100, 600, &rc);
        /* Make sure val is 100, 200, 300, 400, 500 or 600 */
        if (rc || ((val != 100) && (val != 200) && (val != 300) &&
            (val != 400) && (val != 500) && (val != 600))) {
                return tsl2591_shell_err_invalid_arg(argv[2]);
        }
        switch(val) {
            case 100:
                rc = tsl2591_set_integration_time(&g_sensor_itf, TSL2591_LIGHT_ITIME_100MS);
            break;
            case 200:
                rc = tsl2591_set_integration_time(&g_sensor_itf, TSL2591_LIGHT_ITIME_200MS);
            break;
            case 300:
                rc = tsl2591_set_integration_time(&g_sensor_itf, TSL2591_LIGHT_ITIME_300MS);
            break;
            case 400:
                rc = tsl2591_set_integration_time(&g_sensor_itf, TSL2591_LIGHT_ITIME_400MS);
            break;
            case 500:
                rc = tsl2591_set_integration_time(&g_sensor_itf, TSL2591_LIGHT_ITIME_500MS);
            break;
            case 600:
                rc = tsl2591_set_integration_time(&g_sensor_itf, TSL2591_LIGHT_ITIME_600MS);
            break;
        }

        if (rc) {
            console_printf("Setting integration time failed rc:%d", rc);
        }
    }

err:
    return rc;
}

static int
tsl2591_shell_cmd_en(int argc, char **argv)
{
    char *endptr;
    long lval;
    int rc;
    uint8_t enabled;

    if (argc > 3) {
        return tsl2591_shell_err_too_many_args(argv[1]);
    }

    /* Display current enable state */
    if (argc == 2) {
        rc = tsl2591_get_enable(&g_sensor_itf, &enabled);
        if (rc) {
            console_printf("Enable read failure rc:%d", rc);
            goto err;
        }

        console_printf("%u\n", enabled);
    }

    /* Update the enable state */
    if (argc == 3) {
        lval = strtol(argv[2], &endptr, 10); /* Base 10 */
        if (argv[2][0] != '\0' && *endptr == '\0' &&
            lval >= 0 && lval <= 1) {
            rc = tsl2591_enable(&g_sensor_itf, lval);
            if (rc) {
                console_printf("Could not enable sensor rc:%d", rc);
                goto err;
            }
        } else {
            return tsl2591_shell_err_invalid_arg(argv[2]);
        }
    }

    return 0;
err:
    return rc;
}

static int
tsl2591_shell_print_reg(int reg, char *name)
{
    uint8_t val;
    int rc;

    val = 0;
    rc = tsl2591_read8(&g_sensor_itf,
                      TSL2591_COMMAND_BIT | reg,
                      &val);
    if (rc) {
       goto err;
    }
    console_printf("0x%02X (%s): 0x%02X\n", reg, name, val);

    return 0;
err:
    return rc;
}

static int
tsl2591_shell_cmd_dump(int argc, char **argv)
{
    int rc;

    if (argc > 3) {
        return tsl2591_shell_err_too_many_args(argv[1]);
    }

    /* Dump all the register values for debug purposes */
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_ENABLE, "ENABLE");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_CONTROL, "CONTROL");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_THRESHOLD_AILTL, "AILTL");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_THRESHOLD_AILTH, "AILTH");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_THRESHOLD_AIHTL, "AIHTL");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_THRESHOLD_AIHTH, "AIHTH");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_THRESHOLD_NPAILTL, "NPAILTL");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_THRESHOLD_NPAILTH, "NPAILTH");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_THRESHOLD_NPAIHTL, "NPAIHTL");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_THRESHOLD_NPAIHTH, "NPAIHTH");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_PERSIST_FILTER, "FILTER");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_PACKAGE_PID, "PACKAGEID");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_DEVICE_ID, "DEVICEID");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_DEVICE_STATUS, "STATUS");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_CHAN0_LOW, "CHAN0_LOW");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_CHAN0_HIGH, "CHAN0_HIGH");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_CHAN1_LOW, "CHAN1_LOW");
    if (rc) {
       goto err;
    }
    rc = tsl2591_shell_print_reg(TSL2591_REGISTER_CHAN1_HIGH, "CHAN1_HIGH");
    if (rc) {
       goto err;
    }

    return 0;
err:
    console_printf("Read failed rc:%d", rc);
    return rc;
}

static int
tsl2591_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return tsl2591_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return tsl2591_shell_cmd_read(argc, argv);
    }

    /* Gain command */
    if (argc > 1 && strcmp(argv[1], "gain") == 0) {
        return tsl2591_shell_cmd_gain(argc, argv);
    }

    /* Integration time command */
    if (argc > 1 && strcmp(argv[1], "time") == 0) {
        return tsl2591_shell_cmd_time(argc, argv);
    }

    /* Enable */
    if (argc > 1 && strcmp(argv[1], "en") == 0) {
        return tsl2591_shell_cmd_en(argc, argv);
    }

    /* Debug */
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return tsl2591_shell_cmd_dump(argc, argv);
    }

    return tsl2591_shell_err_unknown_arg(argv[1]);
}

int
tsl2591_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&tsl2591_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
