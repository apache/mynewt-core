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
#include "sensor/accel.h"
#include "lis2dw12/lis2dw12.h"
#include "lis2dw12_priv.h"

#if MYNEWT_VAL(LIS2DW12_CLI)

#include "shell/shell.h"
#include "parse/parse.h"

#define LIS2DW12_CLI_FIRST_REGISTER 0x0D
#define LIS2DW12_CLI_LAST_REGISTER 0x3F

static int lis2dw12_shell_cmd(int argc, char **argv);

static struct shell_cmd lis2dw12_shell_cmd_struct = {
    .sc_cmd = "lis2dw12",
    .sc_cmd_func = lis2dw12_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(LIS2DW12_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(LIS2DW12_SHELL_ITF_NUM),
    .si_cs_pin = MYNEWT_VAL(LIS2DW12_SHELL_CSPIN),
    .si_addr = MYNEWT_VAL(LIS2DW12_SHELL_ITF_ADDR)
};

static int
lis2dw12_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lis2dw12_shell_err_too_few_args(char *cmd_name)
{
    console_printf("Error: too few arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lis2dw12_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lis2dw12_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lis2dw12_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", lis2dw12_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tr    [n_samples]\n");
    console_printf("\tchipid\n");
    console_printf("\tdump\n");
    console_printf("\tpeek [reg]\n");
    console_printf("\tpoke [reg value]\n");
    console_printf("\ttest\n");
    
    return 0;
}

static int
lis2dw12_shell_cmd_read_chipid(int argc, char **argv)
{
    int rc;
    uint8_t chipid;

    rc = lis2dw12_read8(&g_sensor_itf, LIS2DW12_REG_WHO_AM_I, &chipid);
    if (rc) {
        goto err;
    }

    console_printf("CHIP_ID:0x%02X\n", chipid);

    return 0;
err:
    return rc;
}

static int
lis2dw12_shell_cmd_read(int argc, char **argv)
{
    uint16_t samples = 1;
    uint16_t val;
    int rc;
    char tmpstr[13];
    int16_t x,y,z;
    float fx,fy,fz;
    uint8_t fs;

    if (argc > 3) {
        return lis2dw12_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return lis2dw12_shell_err_invalid_arg(argv[2]);
        }
        samples = val;
    }

    while(samples--) {

        rc = lis2dw12_get_fs(&g_sensor_itf, &fs);
        if (rc) {
            return rc;
        }
        
        rc = lis2dw12_get_data(&g_sensor_itf, fs,&x, &y, &z);
        if (rc) {
            console_printf("Read failed: %d\n", rc);
            return rc;
        }

        lis2dw12_calc_acc_ms2(x, &fx);
        lis2dw12_calc_acc_ms2(y, &fy);
        lis2dw12_calc_acc_ms2(z, &fz);
                
        console_printf("x:%s ", sensor_ftostr(fx, tmpstr, 13));
        console_printf("y:%s ", sensor_ftostr(fy, tmpstr, 13));
        console_printf("z:%s\n", sensor_ftostr(fz, tmpstr, 13));
    }

    return 0;
}


static void lis2dw12_shell_dump_reg(const char *name, uint8_t addr)
{
    uint8_t val = 0;
    int rc;

    rc = lis2dw12_read8(&g_sensor_itf, addr, &val);
    if (rc == 0) {
        console_printf("0x%02X (%s): 0x%02X\n", addr, name, val);
    } else {
        console_printf("0x%02X (%s): failed (%d)\n", addr, name, rc);
    }
}

#define DUMP_REG(name) lis2dw12_shell_dump_reg(#name, LIS2DW12_REG_ ## name)

static int
lis2dw12_shell_cmd_dump(int argc, char **argv)
{
    if (argc > 2) {
        return lis2dw12_shell_err_too_many_args(argv[1]);
    }

    /* Dump all the register values for debug purposes */
    DUMP_REG(OUT_TEMP_L);
    DUMP_REG(OUT_TEMP_H);
    DUMP_REG(WHO_AM_I);
    DUMP_REG(CTRL_REG1);
    DUMP_REG(CTRL_REG2);
    DUMP_REG(CTRL_REG3);
    DUMP_REG(CTRL_REG4);
    DUMP_REG(CTRL_REG5);
    DUMP_REG(CTRL_REG6);
    DUMP_REG(TEMP_OUT);
    DUMP_REG(STATUS_REG);
    DUMP_REG(OUT_X_L);
    DUMP_REG(OUT_X_H);
    DUMP_REG(OUT_Y_L);
    DUMP_REG(OUT_Y_H);
    DUMP_REG(OUT_Z_L);
    DUMP_REG(OUT_Z_H);
    DUMP_REG(FIFO_CTRL);
    DUMP_REG(FIFO_SAMPLES);
    DUMP_REG(TAP_THS_X);
    DUMP_REG(TAP_THS_Y);
    DUMP_REG(TAP_THS_Z);
    DUMP_REG(INT_DUR);
    DUMP_REG(FREEFALL);
    DUMP_REG(INT_SRC);
    DUMP_REG(X_OFS);
    DUMP_REG(Y_OFS);
    DUMP_REG(Z_OFS);
    DUMP_REG(CTRL_REG7);

    return 0;
}

static int
lis2dw12_shell_cmd_peek(int argc, char **argv)
{
    int rc;
    uint8_t value;
    uint8_t reg;

    if (argc > 3) {
        return lis2dw12_shell_err_too_many_args(argv[1]);
    } else if (argc < 3) {
        return lis2dw12_shell_err_too_few_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], LIS2DW12_CLI_FIRST_REGISTER, LIS2DW12_CLI_LAST_REGISTER, &rc);
    if (rc != 0) {
        return lis2dw12_shell_err_invalid_arg(argv[2]);
    }

    rc = lis2dw12_read8(&g_sensor_itf, reg, &value);
    if (rc) {
        console_printf("peek failed %d\n", rc);
    }else{
        console_printf("reg 0x%02X(%d) = 0x%02X\n", reg, reg, value);
    }

    return 0;
}

static int
lis2dw12_shell_cmd_poke(int argc, char **argv)
{
    int rc;
    uint8_t reg;
    uint8_t value;

    if (argc > 4) {
        return lis2dw12_shell_err_too_many_args(argv[1]);
    } else if (argc < 4) {
        return lis2dw12_shell_err_too_few_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], LIS2DW12_CLI_FIRST_REGISTER, LIS2DW12_CLI_LAST_REGISTER, &rc);
    if (rc != 0) {
        return lis2dw12_shell_err_invalid_arg(argv[2]);
   }

    value = parse_ll_bounds(argv[3], 0, 255, &rc);
    if (rc != 0) {
        return lis2dw12_shell_err_invalid_arg(argv[3]);
    }

    rc = lis2dw12_write8(&g_sensor_itf, reg, value);
    if (rc) {
        console_printf("poke failed %d\n", rc);
    }else{
        console_printf("wrote: 0x%02X(%d) to 0x%02X\n", value, value, reg);
    }

    return 0;
}

static int
lis2dw12_shell_cmd_test(int argc, char **argv)
{
    int rc;
    int result;

    rc = lis2dw12_run_self_test(&g_sensor_itf, &result);
    if (rc) {
        goto err;
    }

    if (result) {
        console_printf("SELF TEST: FAILED\n");        
    } else {
        console_printf("SELF TEST: PASSED\n");        
    }

    return 0;
err:
    return rc;
}

static int
lis2dw12_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return lis2dw12_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return lis2dw12_shell_cmd_read(argc, argv);
    }

    /* Chip ID */
    if (argc > 1 && strcmp(argv[1], "chipid") == 0) {
        return lis2dw12_shell_cmd_read_chipid(argc, argv);
    }

    /* Dump */
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return lis2dw12_shell_cmd_dump(argc, argv);
    }
    
    /* Peek */
    if (argc > 1 && strcmp(argv[1], "peek") == 0) {
        return lis2dw12_shell_cmd_peek(argc, argv);
    }
    
    /* Poke */
    if (argc > 1 && strcmp(argv[1], "poke") == 0) {
        return lis2dw12_shell_cmd_poke(argc, argv);
    }

    /* Test */
    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        return lis2dw12_shell_cmd_test(argc, argv);
    }
    
    return lis2dw12_shell_err_unknown_arg(argv[1]);
}

int
lis2dw12_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&lis2dw12_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
