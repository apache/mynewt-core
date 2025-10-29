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
#include "sensor/accel.h"
#include "ina228/ina228.h"
#include "parse/parse.h"

#if MYNEWT_VAL(INA228_CLI)

static enum ina228_ct vct;
static enum ina228_ct sct;
static enum ina228_avg_mode avg;
static uint8_t soft_avg = 1;

static int
ina228_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n", cmd_name);
    return EINVAL;
}

static int
ina228_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n", cmd_name);
    return EINVAL;
}

static int
ina228_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n", cmd_name);
    return EINVAL;
}

static int
ina228_shell_help(void)
{
    console_printf("ina228 cmd [flags...]\n");
    console_printf("cmd:\n");
    console_printf("\tr [n_samples]\n");
    console_printf("\tavg n\n");
    console_printf("\tsoftavg n\n");
    console_printf("\tsct n\n");
    console_printf("\tvct n\n");

    return 0;
}

const static uint16_t avg_mode[8] = { 1, 4, 16, 64, 128, 256, 512, 1024 };
const static uint16_t ct_mode[8] = { 50, 150, 280, 540, 1052, 2074, 4120 };

static int
ina228_shell_cmd_avg(int argc, char **argv)
{
    int rc = -1;
    long long val = 0;

    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, 7, &rc);
    }
    if (rc == 0) {
        avg = (uint8_t)val;
    }
    console_printf("avg of %u\n", avg_mode[avg]);

    return 0;
}

static int
ina228_shell_cmd_soft_avg(int argc, char **argv)
{
    int rc = -1;
    long long val = 0;

    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 1, 100, &rc);
    }
    if (rc == 0) {
        soft_avg = (uint8_t)val;
    }
    console_printf("softavg of %u\n", soft_avg);

    return 0;
}

static int
ina228_shell_cmd_sct(int argc, char **argv)
{
    int rc = -1;
    long long val = 0;

    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, 7, &rc);
    }
    if (rc == 0) {
        sct = (uint8_t)val;
    }
    console_printf("sct = %u us\n", ct_mode[sct]);

    return 0;
}

static int
ina228_shell_cmd_vct(int argc, char **argv)
{
    int rc = -1;
    long long val = 0;

    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, 7, &rc);
    }
    if (rc == 0) {
        vct = (uint8_t)val;
    }
    console_printf("vct = %u us\n", ct_mode[vct]);

    return 0;
}

static int
ina228_shell_cmd_read(int argc, char **argv)
{
    uint16_t samples = 1;
    uint16_t val;
    int32_t current;
    int32_t current_acc = 0;
    uint32_t vbus;
    uint32_t vbus_acc = 0;
    int32_t temp;
    int i = 0;
    int rc = 0;
    struct ina228_dev *ina228;
    struct ina228_cfg ina228_cfg = {
        .vshct = sct,
        .vbusct = vct,
        .avg_mode = avg,
    };

    if (argc > 3) {
        return ina228_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return ina228_shell_err_invalid_arg(argv[2]);
        }
        samples = val;
    }

    ina228 = (struct ina228_dev *)os_dev_open(MYNEWT_VAL(INA228_SHELL_DEV_NAME),
                                              100, &ina228_cfg);
    if (ina228 == NULL) {
        console_printf("Can't open %s device\n", MYNEWT_VAL(INA228_SHELL_DEV_NAME));
        return 0;
    }

    ina228_start_continuous_mode(ina228, INA228_MODE_CONTINUOUS |
                                 INA228_MODE_SHUNT_VOLTAGE |
                                 INA228_MODE_BUS_VOLTAGE |
                                 INA228_MODE_TEMPERATURE);

    while (rc == 0 && samples) {

        rc = ina228_wait_and_read(ina228, &current, &vbus, NULL, &temp);
        if (rc == SYS_EBUSY) {
            /* Conversion not ready yet, old interrupt fired, wait again. */
            continue;
        }

        if (rc) {
            console_printf("Read failed: %d\n", rc);
            break;
        }

        current_acc += current;
        vbus_acc += vbus;
        if (++i == soft_avg) {
            i = 0;
            --samples;
            current = current_acc / soft_avg;
            vbus = vbus_acc / soft_avg;
            console_printf("current: %5d [uA], vbus = %5u [mV], t = %d.%d C\n",
                           (int)current, (unsigned int)vbus / 1000,
                           (int)temp / 1000, abs(temp) / 100 % 10);
            current_acc = 0;
            vbus_acc = 0;
        }
    }
    (void)ina228_stop_continuous_mode(ina228);

    os_dev_close((struct os_dev *)ina228);

    return 0;
}

static int
ina228_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return ina228_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return ina228_shell_cmd_read(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "avg") == 0) {
        return ina228_shell_cmd_avg(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "softavg") == 0) {
        return ina228_shell_cmd_soft_avg(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "sct") == 0) {
        return ina228_shell_cmd_sct(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "vct") == 0) {
        return ina228_shell_cmd_vct(argc, argv);
    }

    return ina228_shell_err_unknown_arg(argv[1]);
}

MAKE_SHELL_CMD(ina228, ina228_shell_cmd, NULL)

#endif
