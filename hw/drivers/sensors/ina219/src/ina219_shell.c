/*
 * Copyright 2020 Jesus Ipanienko
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <errno.h>
#include "os/mynewt.h"
#include "console/console.h"
#include "shell/shell.h"
#include "sensor/accel.h"
#include "ina219/ina219.h"
#include "parse/parse.h"

#if MYNEWT_VAL(INA219_CLI)

static enum ina219_adc_mode vmod = MYNEWT_VAL(INA219_DEFAULT_VBUS_ADC_MODE);
static enum ina219_adc_mode smod = MYNEWT_VAL(INA219_DEFAULT_VSHUNT_ADC_MODE);
static enum ina219_vbus_full_scale fs = MYNEWT_VAL(INA219_DEFAULT_VBUS_FULL_SACLE);
static uint8_t soft_avg = 1;

static int ina219_shell_cmd(int argc, char **argv);

static struct shell_cmd ina219_shell_cmd_struct = {
    .sc_cmd = "ina219",
    .sc_cmd_func = ina219_shell_cmd
};

static int
ina219_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
ina219_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
ina219_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
ina219_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", ina219_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tr [n_samples]\n");
    console_printf("\tfs [0..1]\n");
    console_printf("\tsoftavg [1..100]\n");
    console_printf("\tsmod [0..15]\n");
    console_printf("\tvmod [0..15]\n");

    return 0;
}

static int
ina219_shell_cmd_fs(int argc, char **argv)
{
    int rc = -1;
    long long val;

    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, 1, &rc);
    }
    if (rc == 0) {
        fs = (uint8_t)val;
    }
    console_printf("fs %d V\n", fs ? 32 : 16);

    return 0;
}

static int
ina219_shell_cmd_soft_avg(int argc, char **argv)
{
    int rc = -1;
    long long val;

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
ina219_shell_cmd_smod(int argc, char **argv)
{
    int rc = -1;
    long long val;

    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, 7, &rc);
    }
    if (rc == 0) {
        smod = (uint8_t)val;
    }
    console_printf("smod = %u\n", smod);

    return 0;
}

static int
ina219_shell_cmd_vmod(int argc, char **argv)
{
    int rc = -1;
    long long val;

    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, 15, &rc);
    }
    if (rc == 0) {
        vmod = (uint8_t)val;
    }
    console_printf("vmod = %u\n", vmod);

    return 0;
}

static int
ina219_shell_cmd_read(int argc, char **argv)
{
    uint16_t samples = 1;
    uint16_t val;
    int32_t current;
    int32_t current_acc = 0;
    uint16_t vbus;
    uint32_t vbus_acc = 0;
    int i = 0;
    int rc = 0;
    struct ina219_dev *ina219;
    struct ina219_cfg ina219_cfg = {
        .vshunt_mode = smod,
        .vbus_mode = vmod,
        .vbus_fs = fs,
    };

    if (argc > 3) {
        return ina219_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return ina219_shell_err_invalid_arg(argv[2]);
        }
        samples = val;
    }

    ina219 = (struct ina219_dev *)os_dev_open(MYNEWT_VAL(INA219_SHELL_DEV_NAME), 100, &ina219_cfg);
    if (ina219 == NULL) {
        console_printf("Can't open %s device\n", MYNEWT_VAL(INA219_SHELL_DEV_NAME));
        return 0;
    }

    ina219_start_continuous_mode(ina219, INA219_OPER_SHUNT_AND_BUS_CONTINUOUS);

    while (rc == 0 && samples) {

        rc = ina219_read_values(ina219, &current, &vbus, NULL);
        if (rc == SYS_EAGAIN) {
            /* Conversion not ready yet, old interrupt fired, wait again. */
            rc = 0;
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
            console_printf("current: %5d [uA], vbus = %5u [mV]\n", (int)current, (unsigned int)vbus);
            current_acc = 0;
            vbus_acc = 0;
        }
    }
    (void)ina219_stop_continuous_mode(ina219);

    os_dev_close((struct os_dev *)ina219);

    return 0;
}

static int
ina219_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return ina219_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return ina219_shell_cmd_read(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "fs") == 0) {
        return ina219_shell_cmd_fs(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "softavg") == 0) {
        return ina219_shell_cmd_soft_avg(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "smod") == 0) {
        return ina219_shell_cmd_smod(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "vmod") == 0) {
        return ina219_shell_cmd_vmod(argc, argv);
    }

    return ina219_shell_err_unknown_arg(argv[1]);
}

int
ina219_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&ina219_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
