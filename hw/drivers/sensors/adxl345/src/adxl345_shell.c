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
#include "adxl345/adxl345.h"
#include "adxl345_priv.h"
#include "parse/parse.h"

#if MYNEWT_VAL(ADXL345_CLI)

static int adxl345_shell_cmd(int argc, char **argv);

static struct shell_cmd adxl345_shell_cmd_struct = {
    .sc_cmd = "adxl345",
    .sc_cmd_func = adxl345_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(ADXL345_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(ADXL345_SHELL_ITF_NUM),
    .si_cs_pin = MYNEWT_VAL(ADXL345_SHELL_CSPIN),
    .si_addr = MYNEWT_VAL(ADXL345_SHELL_ITF_ADDR)
};

static int
adxl345_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
adxl345_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
adxl345_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
adxl345_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", adxl345_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tr    [n_samples]\n");
    console_printf("\tchipid\n");
    console_printf("\tdump\n");

    return 0;
}

static int
adxl345_shell_cmd_read_chipid(int argc, char **argv)
{
    int rc;
    uint8_t chipid;

    rc = adxl345_read8(&g_sensor_itf, ADXL345_DEVID, &chipid);
    if (rc) {
        goto err;
    }

    console_printf("CHIP_ID:0x%02X\n", chipid);

    return 0;
err:
    return rc;
}

static int
adxl345_shell_cmd_read(int argc, char **argv)
{
    uint16_t samples = 1;
    uint16_t val;
    int rc;
    struct sensor_accel_data sample;
    char tmpstr[13];

    if (argc > 3) {
        return adxl345_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return adxl345_shell_err_invalid_arg(argv[2]);
        }
        samples = val;
    }

    while(samples--) {

        rc = adxl345_get_accel_data(&g_sensor_itf, &sample);
        if (rc) {
            console_printf("Read failed: %d\n", rc);
            return rc;
        }

        console_printf("x:%s ", sensor_ftostr(sample.sad_x, tmpstr, 13));
        console_printf("y:%s ", sensor_ftostr(sample.sad_y, tmpstr, 13));
        console_printf("z:%s\n", sensor_ftostr(sample.sad_z, tmpstr, 13));
    }

    return 0;
}

static int
adxl345_shell_cmd_dump(int argc, char **argv)
{
    uint8_t val;
    
    if (argc > 2) {
        return adxl345_shell_err_too_many_args(argv[1]);
    }
    
    /* Dump all the register values for debug purposes */
    val = 0;
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_DEVID, &val));
    console_printf("0x%02X (DEVID): 0x%02X\n", ADXL345_DEVID, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_THRESH_TAP, &val));
    console_printf("0x%02X (THRESH_TAP): 0x%02X\n", ADXL345_THRESH_TAP, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_OFSX, &val));
    console_printf("0x%02X (OFSX): 0x%02X\n", ADXL345_OFSX, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_OFSY, &val));
    console_printf("0x%02X (OFSY): 0x%02X\n", ADXL345_OFSY, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_OFSZ, &val));
    console_printf("0x%02X (OFSZ): 0x%02X\n", ADXL345_OFSZ, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_DUR, &val));
    console_printf("0x%02X (DUR): 0x%02X\n", ADXL345_DUR, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_LATENT, &val));
    console_printf("0x%02X (LATENT): 0x%02X\n", ADXL345_LATENT, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_WINDOW, &val));
    console_printf("0x%02X (WINDOW): 0x%02X\n", ADXL345_WINDOW, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_THRESH_ACT, &val));
    console_printf("0x%02X (THRESH_ACT): 0x%02X\n", ADXL345_THRESH_ACT, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_THRESH_INACT, &val));
    console_printf("0x%02X (THRESH_INACT): 0x%02X\n", ADXL345_THRESH_INACT, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_TIME_INACT, &val));
    console_printf("0x%02X (TIME_INACT): 0x%02X\n", ADXL345_TIME_INACT, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_ACT_INACT_CTL, &val));
    console_printf("0x%02X (ACT_INACT_CTL): 0x%02X\n", ADXL345_ACT_INACT_CTL, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_THRESH_FF, &val));
    console_printf("0x%02X (THRESH_FF): 0x%02X\n", ADXL345_THRESH_FF, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_TIME_FF, &val));
    console_printf("0x%02X (TIME_FF): 0x%02X\n", ADXL345_TIME_FF, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_TAP_AXES, &val));
    console_printf("0x%02X (TAP_AXES): 0x%02X\n", ADXL345_TAP_AXES, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_ACT_TAP_STATUS, &val));
    console_printf("0x%02X (ACT_TAP_STATUS): 0x%02X\n", ADXL345_ACT_TAP_STATUS, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_BW_RATE, &val));
    console_printf("0x%02X (BW_RATE): 0x%02X\n", ADXL345_BW_RATE, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_POWER_CTL, &val));
    console_printf("0x%02X (POWER_CTL): 0x%02X\n", ADXL345_POWER_CTL, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_INT_ENABLE, &val));
    console_printf("0x%02X (INT_ENABLE): 0x%02X\n", ADXL345_INT_ENABLE, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_INT_MAP, &val));
    console_printf("0x%02X (INT_MAP): 0x%02X\n", ADXL345_INT_MAP, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_INT_SOURCE, &val));
    console_printf("0x%02X (INT_SOURCE): 0x%02X\n", ADXL345_INT_SOURCE, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_DATA_FORMAT, &val));
    console_printf("0x%02X (DATA_FORMAT): 0x%02X\n", ADXL345_DATA_FORMAT, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_DATAX0, &val));
    console_printf("0x%02X (DATAX0): 0x%02X\n", ADXL345_DATAX0, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_DATAX1, &val));
    console_printf("0x%02X (DATAX1): 0x%02X\n", ADXL345_DATAX1, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_DATAX0, &val));
    console_printf("0x%02X (DATAY0): 0x%02X\n", ADXL345_DATAX0, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_DATAX1, &val));
    console_printf("0x%02X (DATAY1): 0x%02X\n", ADXL345_DATAX1, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_DATAY0, &val));
    console_printf("0x%02X (DATAZ0): 0x%02X\n", ADXL345_DATAY0, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_DATAZ1, &val));
    console_printf("0x%02X (DATAZ1): 0x%02X\n", ADXL345_DATAZ1, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_FIFO_CTL, &val));
    console_printf("0x%02X (FIFO_CTL): 0x%02X\n", ADXL345_FIFO_CTL, val);
    assert(0 == adxl345_read8(&g_sensor_itf, ADXL345_FIFO_STATUS, &val));
    console_printf("0x%02X (FIFO_STATUS): 0x%02X\n", ADXL345_FIFO_STATUS, val);
    
    return 0;
}

static int
adxl345_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return adxl345_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return adxl345_shell_cmd_read(argc, argv);
    }

    /* Chip ID */
    if (argc > 1 && strcmp(argv[1], "chipid") == 0) {
        return adxl345_shell_cmd_read_chipid(argc, argv);
    }

    /* Dump */
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return adxl345_shell_cmd_dump(argc, argv);
    }

    return adxl345_shell_err_unknown_arg(argv[1]);
}

int
adxl345_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&adxl345_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
