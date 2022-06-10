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
#include "lsm6dsl/lsm6dsl.h"
#include "lsm6dsl_priv.h"
#include "parse/parse.h"

typedef struct {
    uint8_t addr;
    char *regname;
} reg_name_t;

static int lsm6dsl_shell_cmd(int argc, char **argv);

/* Human readable register map for page 0 */
static const reg_name_t reg_name[] = {
    { .addr = 0x02, .regname = "FUNC_CFG_ACCESS" },
    { .addr = 0x04, .regname = "SENSOR_SYNC_TIME_FRAME" },
    { .addr = 0x05, .regname = "SENSOR_SYNC_RES_RATIO" },
    { .addr = 0x06, .regname = "FIFO_CTRL1" },
    { .addr = 0x07, .regname = "FIFO_CTRL2" },
    { .addr = 0x08, .regname = "FIFO_CTRL3" },
    { .addr = 0x09, .regname = "FIFO_CTRL4" },
    { .addr = 0x0a, .regname = "FIFO_CTRL5" },
    { .addr = 0x0b, .regname = "DRDY_PULSE_CFG_G" },
    { .addr = 0x0d, .regname = "INT1_CTRL" },
    { .addr = 0x0e, .regname = "INT2_CTRL" },
    { .addr = 0x0f, .regname = "WHO_AM_I" },
    { .addr = 0x10, .regname = "CTRL1_XL" },
    { .addr = 0x11, .regname = "CTRL2_G" },
    { .addr = 0x12, .regname = "CTRL3_C" },
    { .addr = 0x13, .regname = "CTRL4_C" },
    { .addr = 0x14, .regname = "CTRL5_C" },
    { .addr = 0x15, .regname = "CTRL6_C" },
    { .addr = 0x16, .regname = "CTRL7_G" },
    { .addr = 0x17, .regname = "CTRL8_XL" },
    { .addr = 0x18, .regname = "CTRL9_XL" },
    { .addr = 0x19, .regname = "CTRL10_C" },
    { .addr = 0x1A, .regname = "MASTER_CONFIG" },
    { .addr = 0x1B, .regname = "WAKE_UP_SRC" },
    { .addr = 0x1C, .regname = "TAP_SRC" },
    { .addr = 0x1D, .regname = "D6D_SRC" },
    { .addr = 0x1E, .regname = "STATUS_REG" },
    { .addr = 0x3a, .regname = "FIFO_STATUS1" },
    { .addr = 0x3b, .regname = "FIFO_STATUS2" },
    { .addr = 0x3c, .regname = "FIFO_STATUS3" },
    { .addr = 0x3d, .regname = "FIFO_STATUS4" },
    { .addr = 0x53, .regname = "FUNC_SRC1" },
    { .addr = 0x54, .regname = "FUNC_SRC2" },
    { .addr = 0x55, .regname = "WRIST_TILT_IA" },
    { .addr = 0x58, .regname = "TAP_CFG" },
    { .addr = 0x59, .regname = "TAP_THS_6D" },
    { .addr = 0x5a, .regname = "INT_DUR2" },
    { .addr = 0x5b, .regname = "WAKE_UP_THS" },
    { .addr = 0x5c, .regname = "WAKE_UP_DUR" },
    { .addr = 0x5d, .regname = "FREE_FALL" },
    { .addr = 0x5e, .regname = "MD1_CFG" },
    { .addr = 0x5f, .regname = "MD2_CFG" },
};

/* Human readable register map for bankA */
static const reg_name_t reg_name_banka[] = {
    { .addr = 0x0f, .regname = "CONFIG_PEDO_THS_MIN" },
    { .addr = 0x13, .regname = "SM_THS" },
    { .addr = 0x14, .regname = "PEDO_DEB_REG" },
    { .addr = 0x15, .regname = "STEP_COUNT_DELTA" },
};

/* Human readable register map for bankB */
static const reg_name_t reg_name_bankb[] = {
    { .addr = 0x50, .regname = "A_WRIST_TILT_LAT" },
    { .addr = 0x54, .regname = "A_WRIST_TILT_THS" },
    { .addr = 0x59, .regname = "A_WRIST_TILT_Mask" },
};

static struct shell_cmd lsm6dsl_shell_cmd_struct = {
    .sc_cmd = "lsm6dsl",
    .sc_cmd_func = lsm6dsl_shell_cmd
};

static struct lsm6dsl *g_lsm6dsl;

static int
lsm6dsl_shell_open_device(void)
{
    if (!g_lsm6dsl) {
        g_lsm6dsl = (struct lsm6dsl *)os_dev_open(MYNEWT_VAL(LSM6DSL_SHELL_DEV_NAME),
                                                  1000, NULL);
    }

    return g_lsm6dsl ? 0 : SYS_ENODEV;
}

static int
lsm6dsl_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lsm6dsl_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lsm6dsl_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static void
lsm6dsl_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", lsm6dsl_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tdump\t[START\tEND]\n");
    console_printf("\tread\tADD\n");
    console_printf("\twrite\tADD\tDATA\n");
    console_printf("\ttest\n");
}

static const reg_name_t *
lsm6dsl_get_reg(int i)
{
    int j;

    for (j = 0; j < ARRAY_SIZE(reg_name); j++)
        if (i == reg_name[j].addr) {
            return &reg_name[j];
        }

    return NULL;
}

static int
lsm6dsl_shell_cmd_dump(int argc, char **argv)
{
    int rc = 0;
    uint8_t value;
    int i;
    int sreg, ereg;
    const reg_name_t *regname;
    bool all;

    if (argc > 4) {
        return lsm6dsl_shell_err_too_many_args(argv[1]);
    }

    all = argc == 3 && 0 == strcmp(argv[2], "all");

    if (argc == 2 || all) {
        for (i = 0; i < ARRAY_SIZE(reg_name); i++) {
            rc = lsm6dsl_read(g_lsm6dsl, reg_name[i].addr, &value, 1);
            if (rc) {
                console_printf("dump failed %d\n", rc);
            } else if (all || value != 0){
                console_printf("%-22s(0x%02X) = 0x%02X\n",
                               reg_name[i].regname, reg_name[i].addr, value);
            }
        }

        /* Bank A */
        rc = lsm6dsl_write_reg(g_lsm6dsl, LSM6DSL_FUNC_CFG_ACCESS_REG, LSM6DSL_FUNC_CFG_ACCESS_MASK);
        if (rc) {
            return rc;
        }
        for (i = 0; i < ARRAY_SIZE(reg_name_banka); i++) {
            rc = lsm6dsl_read(g_lsm6dsl, reg_name_banka[i].addr, &value, 1);
            if (rc) {
                console_printf("dump failed %d\n", rc);
            } else if (all || value != 0){
                console_printf("%-22s(0x%02X) = 0x%02X\n",
                               reg_name_banka[i].regname, reg_name_banka[i].addr, value);
            }
        }
        rc = lsm6dsl_write_reg(g_lsm6dsl, LSM6DSL_FUNC_CFG_ACCESS_REG, 0);
        if (rc) {
            return rc;
        }

        /* Bank B */
        rc = lsm6dsl_write_reg(g_lsm6dsl, LSM6DSL_FUNC_CFG_ACCESS_REG, LSM6DSL_FUNC_CFG_ACCESS_MASK | LSM6DSL_SHUB_REG_ACCESS_MASK);
        if (rc) {
            return rc;
        }
        for (i = 0; i < ARRAY_SIZE(reg_name_bankb); i++) {
            rc = lsm6dsl_read(g_lsm6dsl, reg_name_bankb[i].addr, &value, 1);
            if (rc) {
                console_printf("dump failed %d\n", rc);
            } else if (all || value != 0){
                console_printf("%-22s(0x%02X) = 0x%02X\n",
                               reg_name_bankb[i].regname, reg_name_bankb[i].addr, value);
            }
        }
        rc = lsm6dsl_write_reg(g_lsm6dsl, LSM6DSL_FUNC_CFG_ACCESS_REG, 0);
        if (rc) {
            return rc;
        }
    } else {
        sreg = parse_ll_bounds(argv[2], 0x02, 0x7F, &rc);
        if (rc != 0) {
            return lsm6dsl_shell_err_invalid_arg(argv[2]);
        }

        ereg = parse_ll_bounds(argv[3], 0x02, 0x7F, &rc);
        if (rc != 0) {
            return lsm6dsl_shell_err_invalid_arg(argv[3]);
        }

        for (i = sreg; i <= ereg; i++) {
            rc = lsm6dsl_read(g_lsm6dsl, i, &value, 1);
            if (rc) {
                console_printf("dump failed %d\n", rc);
            } else {
                regname = lsm6dsl_get_reg(i);
                if (regname)
                    console_printf("reg %-22s(0x%02X) = 0x%02X\n",
                                   regname->regname, i, value);
                else
                    console_printf("reg 0x%02X = 0x%02X\n", i, value);
            }
        }
    }

    return rc;
}

static int
lsm6dsl_shell_cmd_read(int argc, char **argv)
{
    int rc;
    uint8_t value;
    int reg;

    if (argc > 3) {
        return lsm6dsl_shell_err_too_many_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], 0x02, 0x7F, &rc);
    if (rc != 0) {
        return lsm6dsl_shell_err_invalid_arg(argv[2]);
   }

    rc = lsm6dsl_read(g_lsm6dsl, reg, &value, 1);
    if (rc) {
        console_printf("read failed %d\n", rc);
    } else {
        console_printf("reg 0x%02X(%d) = 0x%02X\n", reg, reg, value);
    }

    return rc;
}

static int
lsm6dsl_shell_cmd_write(int argc, char **argv)
{
    int rc;
    uint8_t value;
    int reg;

    if (argc > 4) {
        return lsm6dsl_shell_err_too_many_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], 0x02, 0x7F, &rc);
    if (rc != 0) {
        return lsm6dsl_shell_err_invalid_arg(argv[2]);
    }

    value = parse_ll_bounds(argv[3], 0x00, 0xFF, &rc);
    if (rc != 0) {
       return lsm6dsl_shell_err_invalid_arg(argv[2]);
    }

    rc = lsm6dsl_write(g_lsm6dsl, reg, &value, 1);
    if (rc) {
        console_printf("write failed %d\n", rc);
    }

    return rc;
}

static int
lsm6dsl_shell_cmd_test(int argc, char **argv)
{
    int rc;
    int result;

    rc = lsm6dsl_run_self_test(g_lsm6dsl, &result);
    if (rc) {
        console_printf("test not started %d\n", rc);
    } else {
        console_printf("Test Result: %x\n", result);
    }

    return rc;
}

static int
lsm6dsl_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        lsm6dsl_shell_help();

        return 0;
    }

    if (lsm6dsl_shell_open_device()) {
        console_printf("Error: device not found \"%s\"\n",
                       MYNEWT_VAL(LSM6DSL_SHELL_DEV_NAME));
    }

    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return lsm6dsl_shell_cmd_dump(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "read") == 0) {
        return lsm6dsl_shell_cmd_read(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "write") == 0) {
        return lsm6dsl_shell_cmd_write(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        return lsm6dsl_shell_cmd_test(argc, argv);
    }

    return lsm6dsl_shell_err_unknown_arg(argv[1]);
}

int
lsm6dsl_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&lsm6dsl_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

void lsm6dsl_pkg_init(void)
{
#if MYNEWT_VAL(LSM6DSL_CLI)
    (void)lsm6dsl_shell_init();
#endif
}
