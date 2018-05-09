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
#include <os/mynewt.h>
#include <console/console.h>
#include <shell/shell.h>
#include <adp5061/adp5061.h>
#include <parse/parse.h>
#include "adp5061_priv.h"

#if MYNEWT_VAL(ADP5061_CLI)

static struct adp5061_dev *adp5061_dev;

/**
* ADP5061 Register structure
*/
struct adp5061_reg {
    uint8_t addr;
    const char *name;
};

/**
* Definition of all Register names
*/
static const struct adp5061_reg adp5061_device_regs[] = {
    {.addr = REG_PART_ID,           .name = "PART_ID"               },
    {.addr = REG_SILICON_REV,       .name = "SILICON_REV"           },
    {.addr = REG_VIN_PIN_SETTINGS,  .name = "VINx_PIN_SETTINGS"     },
    {.addr = REG_TERM_SETTINGS,     .name = "TERMINATION_SETTINGS"  },
    {.addr = REG_CHARGING_CURRENT,  .name = "CHARGING_CURRENT"      },
    {.addr = REG_VOLTAGE_THRES,     .name = "VOLTAGE_THRESHOLD"     },
    {.addr = REG_TIMER_SETTINGS,    .name = "TIMER_SETTINGS"        },
    {.addr = REG_FUNC_SETTINGS_1,   .name = "FUNCTIONAL_SETTINGS_1" },
    {.addr = REG_FUNC_SETTINGS_2,   .name = "FUNCTIONAL_SETTINGS_2" },
    {.addr = REG_INT_EN,            .name = "INTERRUPT_EN"          },
    {.addr = REG_INT_ACTIVE,        .name = "INTERRUPT_ACTIVE"      },
    {.addr = REG_CHARGER_STATUS_1,  .name = "CHARGER_STATUS_1"      },
    {.addr = REG_CHARGER_STATUS_2,  .name = "CHARGER_STATUS_2"      },
    {.addr = REG_FAULT_REGISTER,    .name = "FAULT_REGISTER"        },
    {.addr = REG_BATT_SHORT,        .name = "BATTERY_SHORT"         },
    {.addr = REG_IEND,              .name = "IEND"                  },
};
#define NUM_DEVICE_REGS (sizeof(adp5061_device_regs)\
                        /sizeof(adp5061_device_regs[0]))

static int adp5061_shell_cmd(int argc, char **argv);

static const struct shell_cmd adp5061_shell_cmd_struct = {
    .sc_cmd = "adp5061",
    .sc_cmd_func = adp5061_shell_cmd
};

static int
adp5061_shell_err_too_many_args(const char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return SYS_EINVAL;
}

static int
adp5061_shell_err_unknown_arg(const char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return SYS_EINVAL;
}

static int
adp5061_shell_err_invalid_arg(const char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return SYS_EINVAL;
}

static int
adp5061_shell_err_missing_arg(const char *cmd_name)
{
    console_printf("Error: missing %s\n", cmd_name);
    return SYS_EINVAL;
}

static int
adp5061_shell_help(void)
{
    // TODO:
    console_printf("%s cmd\n", adp5061_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tdump\n");
    console_printf("\tread reg\n");
    console_printf("\twrite reg\n");
    console_printf("\tdisable\n");
    console_printf("\tenable\n");

    return 0;
}

static int
adp5061_shell_cmd_dump(int argc, char **argv)
{
    uint8_t reg_val;
    int i;

    if (argc > 2) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }

    console_printf("========== Device Regs ==========\n");
    console_printf("==========   ADP5061   ==========\n");
    console_printf("=================================\n\n");
    for (i = 0; i < NUM_DEVICE_REGS; ++i) {
        if (adp5061_get_reg(adp5061_dev,
                adp5061_device_regs[i].addr, &reg_val)) {
            console_printf("%21s [0x%02x] = READ ERROR \n",
                    adp5061_device_regs[i].name, adp5061_device_regs[i].addr);
        } else {
            console_printf("%21s [0x%02x] = 0x%02x \n",
                    adp5061_device_regs[i].name,
                    adp5061_device_regs[i].addr, reg_val);
        }
    }

    return 0;
}

static int
adp5061_shell_read_reg(int argc, char **argv)
{
    uint8_t reg_val;
    uint8_t reg_ix;
    int i;
    int status;

    if (argc < 3) {
        return adp5061_shell_err_missing_arg("register name of address");
    }
    if (argc > 3) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }

    reg_ix = (uint8_t)parse_ull_bounds(argv[2], REG_PART_ID, REG_IEND,
            &status);

    for (i = 0; i < NUM_DEVICE_REGS; ++i) {
        if ((status == 0 && adp5061_device_regs[i].addr == reg_ix) ||
            (strcmp(adp5061_device_regs[i].name, argv[3]) == 0)) {
            if (adp5061_get_reg(adp5061_dev,
                    adp5061_device_regs[i].addr, &reg_val)) {
                console_printf("%21s [0x%02x] = READ ERROR \n",
                        adp5061_device_regs[i].name,
                        adp5061_device_regs[i].addr);
            } else {
                console_printf("%21s [0x%02x] = 0x%02x \n",
                        adp5061_device_regs[i].name,
                        adp5061_device_regs[i].addr, reg_val);
            }
            return 0;
        }
    }
    adp5061_shell_err_invalid_arg(argv[2]);

    return 0;
}

static int
adp5061_shell_write_reg(int argc, char **argv)
{
    uint8_t reg_val;
    uint8_t reg_ix;
    int i;
    int status;
    int rc;

    if (argc < 3) {
        return adp5061_shell_err_missing_arg("register name of address");
    }
    if (argc < 4) {
        return adp5061_shell_err_missing_arg("register value");
    }
    if (argc > 4) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }

    reg_val = (uint8_t)parse_ull_bounds(argv[3], 0, 0xFF, &status);
    if (status) {
        console_printf("Invalid register value\n");
        return status;
    }
    reg_ix = (uint8_t)parse_ull_bounds(argv[2], REG_PART_ID, REG_IEND,
            &status);

    for (i = 0; i < NUM_DEVICE_REGS; ++i) {
        if ((status == 0 && adp5061_device_regs[i].addr == reg_ix) ||
            (strcmp(adp5061_device_regs[i].name, argv[3]) == 0)) {
            rc = adp5061_set_reg(adp5061_dev, adp5061_device_regs[i].addr,
                    reg_val);
            if (rc) {
                console_printf("Write register failed 0x%X\n", rc);
            } else {
                console_printf("Register written\n");
            }
            return 0;
        }
    }
    adp5061_shell_err_invalid_arg(argv[2]);

    return 0;
}

static int
adp5061_shell_enable(int argc, char **argv)
{
    int rc;

    if (argc > 2) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }

    rc = adp5061_charge_enable(adp5061_dev);
    if (rc) {
        console_printf("Charger enable failed 0x%X\n", rc);
    } else {
        console_printf("Charger enabled\n");
    }

    return 0;
}

static int
adp5061_shell_disable(int argc, char **argv)
{
    int rc;

    if (argc > 2) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }

    rc = adp5061_charge_disable(adp5061_dev);
    if (rc) {
        console_printf("Charger disable failed 0x%X\n", rc);
    } else {
        console_printf("Charger disabled\n");
    }

    return 0;
}

static const char *const charger_status_desc[] = {
        "charger off",
        "trickle charge",
        "fast charge (constant current)",
        "fast charge (constant voltage)",
        "charge complete",
        "ldo mode",
        "timer expired",
        "battery detection",
};

static const char *const thermal_status_desc[] = {
        "off",
        "battery cold",
        "battery cool",
        "battery warm",
        "battery hot",
        "0x5",
        "0x6",
        "thermistor OK",
};

static const char *const battery_status_desc[] = {
        "battery monitor off",
        "no battery",
        "Vbat_sns < Vtrk",
        "Vtrk <= Vbat_sns < Vweak",
        "Vbat_sns >= Vweak",
        "0x5",
        "0x6",
        "0x7",
};

static int
adp5061_shell_charger_status(int argc, char **argv)
{
    int rc = 0;
    uint8_t status_1;
    uint8_t status_2;

    if (argc > 2) {
        rc = adp5061_shell_err_too_many_args(argv[1]);
        goto err;
    }

    rc = adp5061_get_reg(adp5061_dev, REG_CHARGER_STATUS_1, &status_1);
    if (rc) {
        console_printf("Read charger status 1 failed %d\n", rc);
        goto err;
    }
    console_printf("CHARGER_STATUS: %s\n",
        charger_status_desc[ADP5061_CHG_STATUS_1_GET(status_1)]);
    console_printf("VIN_OK: %d\n",
            ADP5061_CHG_STATUS_1_VIN_OK_GET(status_1));
    console_printf("VIN_OV: %d\n",
            ADP5061_CHG_STATUS_1_VIN_OV_GET(status_1));
    console_printf("VIN_ILIM: %d\n",
            ADP5061_CHG_STATUS_1_VIN_ILIM_GET(status_1));
    console_printf("THERM_ILIM: %d\n",
            ADP5061_CHG_STATUS_1_THERM_LIM_GET(status_1));
    console_printf("CHDONE: %d\n",
            ADP5061_CHG_STATUS_1_CHDONE_GET(status_1));

    rc = adp5061_get_reg(adp5061_dev, REG_CHARGER_STATUS_2, &status_2);
    if (rc) {
        console_printf("Read charger status 1 failed %d\n", rc);
        goto err;
    }
    console_printf("THR_STATUS: %s\n",
            thermal_status_desc[ADP5061_CHG_STATUS_2_THR_GET(status_2)]);
    console_printf("RCH_LIM_INFO: %d\n",
            ADP5061_CHG_STATUS_2_RCH_LIM_GET(status_2));
    console_printf("BATTERY_STATUS: %s\n",
            battery_status_desc[ADP5061_CHG_STATUS_2_BATT_GET(status_2)]);
err:
    return rc;
}

static int
adp5061_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return adp5061_shell_help();
    }

    if (strcmp(argv[1], "dump") == 0) {
        return adp5061_shell_cmd_dump(argc, argv);
    } else if (strcmp(argv[1], "read") == 0) {
        return adp5061_shell_read_reg(argc, argv);
    } else if (strcmp(argv[1], "write") == 0) {
        return adp5061_shell_write_reg(argc, argv);
    } else if (strcmp(argv[1], "enable") == 0) {
        return adp5061_shell_enable(argc, argv);
    } else if (strcmp(argv[1], "disable") == 0) {
        return adp5061_shell_disable(argc, argv);
    } else if (strcmp(argv[1], "status") == 0) {
        return adp5061_shell_charger_status(argc, argv);
    }

    return adp5061_shell_err_unknown_arg(argv[1]);
}

int
adp5061_shell_init(struct adp5061_dev *dev)
{
    int rc;

    adp5061_dev = dev;
    rc = shell_cmd_register(&adp5061_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
