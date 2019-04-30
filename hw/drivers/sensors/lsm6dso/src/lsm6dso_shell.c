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
#include "lsm6dso/lsm6dso.h"
#include "lsm6dso_priv.h"
#include "parse/parse.h"

typedef struct {
    uint8_t add;
    char *regname;
} reg_name_t;

static int lsm6dso_shell_cmd(int argc, char **argv);

/* Human readable register map for page 0 */
static const reg_name_t reg_name[] = {
    { .add = 0x02, .regname = "PIN_CTRL" },
    { .add = 0x07, .regname = "FIFO_CTRL1" },
    { .add = 0x08, .regname = "FIFO_CTRL2" },
    { .add = 0x09, .regname = "FIFO_CTRL3" },
    { .add = 0x0a, .regname = "FIFO_CTRL4" },
    { .add = 0x0d, .regname = "INT1_CTRL" },
    { .add = 0x0e, .regname = "INT2_CTRL" },
    { .add = 0x0f, .regname = "WHOAMI" },
    { .add = 0x10, .regname = "CTRL1_XL" },
    { .add = 0x11, .regname = "CTRL2_G" },
    { .add = 0x12, .regname = "CTRL3_C" },
    { .add = 0x13, .regname = "CTRL4_C" },
    { .add = 0x14, .regname = "CTRL5_C" },
    { .add = 0x15, .regname = "CTRL6_C" },
    { .add = 0x16, .regname = "CTRL7_G" },
    { .add = 0x17, .regname = "CTRL8_XL" },
    { .add = 0x18, .regname = "CTRL9_XL" },
    { .add = 0x19, .regname = "CTRL10_C" },
    { .add = 0x1A, .regname = "ALL_INT_SRC" },
    { .add = 0x1B, .regname = "WAKE_UP_SRC" },
    { .add = 0x1C, .regname = "TAP_SRC" },
    { .add = 0x1D, .regname = "D6D_SRC" },
    { .add = 0x1E, .regname = "STATUS_REG" },
    { .add = 0x56, .regname = "TAP_CFG0" },
    { .add = 0x57, .regname = "TAP_CFG1" },
    { .add = 0x58, .regname = "TAP_CFG2" },
    { .add = 0x59, .regname = "TAP_THS_6D" },
    { .add = 0x5a, .regname = "INT_DUR2" },
    { .add = 0x5b, .regname = "WAKE_UP_THS" },
    { .add = 0x5c, .regname = "WAKE_UP_DUR" },
    { .add = 0x5d, .regname = "FREE_FALL" },
    { .add = 0x5e, .regname = "MD1_CFG" },
    { .add = 0x5f, .regname = "MD2_CFG" },
};

static struct shell_cmd lsm6dso_shell_cmd_struct = {
    .sc_cmd = "lsm6dso",
    .sc_cmd_func = lsm6dso_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(LSM6DSO_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(LSM6DSO_SHELL_ITF_NUM),
    .si_cs_pin = MYNEWT_VAL(LSM6DSO_SHELL_CSPIN),
    .si_addr = MYNEWT_VAL(LSM6DSO_SHELL_ITF_ADDR)
};

static int lsm6dso_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int lsm6dso_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int lsm6dso_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static void lsm6dso_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", lsm6dso_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tdump\tSTART\tEND\n");
    console_printf("\tread\tADD\n");
    console_printf("\twrite\tADD\tDATA\n");
    console_printf("\ttest\n");
}

static const reg_name_t *lsm6dso_get_reg(int i)
{
    int j;

    for (j = 0; j < sizeof(reg_name)/sizeof(reg_name[0]); j++)
        if (i == reg_name[j].add) {
            return &reg_name[j];
        }

    return NULL;
}

static int lsm6dso_shell_cmd_dump(int argc, char **argv)
{
    int rc;
    uint8_t value;
    int i;
    int sreg, ereg;
    const reg_name_t *regname;

    if (argc > 4) {
        return lsm6dso_shell_err_too_many_args(argv[1]);
    }

    sreg = parse_ll_bounds(argv[2], 0x02, 0x7F, &rc);
    if (rc != 0) {
        return lsm6dso_shell_err_invalid_arg(argv[2]);
    }

    ereg = parse_ll_bounds(argv[3], 0x02, 0x7F, &rc);
    if (rc != 0) {
        return lsm6dso_shell_err_invalid_arg(argv[3]);
    }

    for (i = sreg; i <= ereg; i++) {
        rc = lsm6dso_readlen(&g_sensor_itf, i, &value, 1);
        if (rc) {
            console_printf("dump failed %d\n", rc);
        } else {
            regname = lsm6dso_get_reg(i);
            if (regname)
                console_printf("reg %s(0x%02X) = 0x%02X\n",
                               regname->regname, i, value);
            else
                console_printf("reg 0x%02X = 0x%02X\n", i, value);
        }
    }

    return rc;
}

static int lsm6dso_shell_cmd_read(int argc, char **argv)
{
    int rc;
    uint8_t value;
    int reg;

    if (argc > 3) {
        return lsm6dso_shell_err_too_many_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], 0x02, 0x7F, &rc);
    if (rc != 0) {
        return lsm6dso_shell_err_invalid_arg(argv[2]);
   }

    rc = lsm6dso_readlen(&g_sensor_itf, reg, &value, 1);
    if (rc) {
        console_printf("read failed %d\n", rc);
    } else {
        console_printf("reg 0x%02X(%d) = 0x%02X\n", reg, reg, value);
    }

    return rc;
}

static int lsm6dso_shell_cmd_write(int argc, char **argv)
{
    int rc;
    uint8_t value;
    int reg;

    if (argc > 4) {
        return lsm6dso_shell_err_too_many_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], 0x02, 0x7F, &rc);
    if (rc != 0) {
        return lsm6dso_shell_err_invalid_arg(argv[2]);
    }

    value = parse_ll_bounds(argv[3], 0x00, 0xFF, &rc);
    if (rc != 0) {
       return lsm6dso_shell_err_invalid_arg(argv[2]);
    }

    rc = lsm6dso_writelen(&g_sensor_itf, reg, &value, 1);
    if (rc) {
        console_printf("write failed %d\n", rc);
    }

    return rc;
}

static int lsm6dso_shell_cmd_test(int argc, char **argv)
{
    int rc;
    int result;

    rc = lsm6dso_run_self_test(&g_sensor_itf, &result);
    if (rc) {
        console_printf("test not started %d\n", rc);
    } else {
        console_printf("Test Result: %x\n", result);
    }

    return rc;
}

static int lsm6dso_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        lsm6dso_shell_help();

        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return lsm6dso_shell_cmd_dump(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "read") == 0) {
        return lsm6dso_shell_cmd_read(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "write") == 0) {
        return lsm6dso_shell_cmd_write(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        return lsm6dso_shell_cmd_test(argc, argv);
    }

    return lsm6dso_shell_err_unknown_arg(argv[1]);
}

int lsm6dso_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&lsm6dso_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}
