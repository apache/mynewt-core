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
#include "drv2605_priv.h"
#include "drv2605/drv2605.h"
#include "sensor/sensor.h"
#include "parse/parse.h"

#if MYNEWT_VAL(DRV2605_CLI)

static int drv2605_shell_cmd(int argc, char **argv);

static struct shell_cmd drv2605_shell_cmd_struct = {
    .sc_cmd = "drv2605",
    .sc_cmd_func = drv2605_shell_cmd
};

static int
drv2605_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
drv2605_shell_err_too_few_args(char *cmd_name)
{
    console_printf("Error: too few arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
drv2605_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
drv2605_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
drv2605_shell_help(void)
{
    console_printf("%s cmd  [flags...]\n", drv2605_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tchip_id\n");
    console_printf("\tload_cal [brake_factor loop_gain lra_sample_time lra_blanking_time lra_idiss_time auto_cal_time lra_zc_det_time]\n");
    console_printf("\tload_rom [up to 8 uint8_t]\n");
    console_printf("\top_mode [reset | rom | pwm | analog | rtp | diag | cal]\n");
    console_printf("\tpower_mode [deep | standby | active]\n");
    console_printf("\ttrigger\n");
    console_printf("\tpeek [reg]\n");
    console_printf("\tpoke [reg value]\n");
    console_printf("\tdump_cal\n");
    console_printf("\tdump_all\n");
    return 0;
}

static int
drv2605_shell_cmd_load_rom(int argc, char **argv, struct drv2605 *drv2605)
{
    int rc;
    uint8_t waveform[8] = {0};
    uint8_t len = argc-2;
    uint8_t i;
    struct sensor_itf *itf;

    if (argc > 10) {
        return drv2605_shell_err_too_many_args(argv[1]);
    } else if (argc < 3) {
        return drv2605_shell_err_too_few_args(argv[1]);
    }

    for (i = 0; i<len;i++) {
        waveform[i] = parse_ll_bounds(argv[i+2], 0, 255, &rc);
        if (rc != 0) {
            goto err;
        }
    }

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    rc = drv2605_load_rom(itf, waveform, len);
    if (rc) {
        console_printf("load failed %d\n", rc);
    }else{
        console_printf("load succeeded\n");
    }

    return 0;
err:
    return rc;
}

static int
drv2605_shell_cmd_trigger_rom(int argc, char **argv, struct drv2605 *drv2605)
{
    int rc;
    struct sensor_itf *itf;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    rc = drv2605_trigger_rom(itf);
    if (rc) {
        console_printf("trigger failed %d\n", rc);
    }else{
        console_printf("trigger succeeded\n");
    }

    return rc;
}

static int
drv2605_shell_cmd_get_chip_id(int argc, char **argv, struct drv2605 *drv2605)
{
    uint8_t id;
    int rc;
    struct sensor_itf *itf;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    /* Display the chip id */
    if (argc == 2) {
        rc = drv2605_get_chip_id(itf, &id);
        if (rc) {
            console_printf("chipid failed %d\n", rc);
        }else{
            console_printf("0x%02X\n", id);
        }
    }

    return 0;
}

static int
drv2605_shell_cmd_dump_cal(int argc, char **argv, struct drv2605 *drv2605)
{
    int rc;
    uint8_t tmp[3];
    struct sensor_itf *itf;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    rc = drv2605_readlen(itf, DRV2605_AUTO_CALIBRATION_COMPENSATION_RESULT_ADDR, &tmp[0], sizeof(tmp));
    if (rc) {
        console_printf("dump failed %d\n", rc);
        goto err;
    }
    console_printf("\nDRV2605_CALIBRATED_COMP: 0x%02X\nDRV2605_CALIBRATED_BEMF: 0x%02X\nDRV2605_CALIBRATED_BEMF_GAIN: %0d\n", tmp[0], tmp[1], tmp[2] & 0x03);

    return 0;
err:
    return rc;
}

static int
drv2605_shell_cmd_peek(int argc, char **argv, struct drv2605 *drv2605)
{
    int rc;
    uint8_t value;
    uint8_t reg;
    struct sensor_itf *itf;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    } else if (argc < 3) {
        return drv2605_shell_err_too_few_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], 0, 34, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    rc = drv2605_read8(itf, reg, &value);
    if (rc) {
        console_printf("peek failed %d\n", rc);
    }else{
        console_printf("value: 0x%02X\n", value);
    }

    return 0;
}

static int
drv2605_shell_cmd_poke(int argc, char **argv, struct drv2605 *drv2605)
{
    int rc;
    uint8_t reg;
    uint8_t value;
    struct sensor_itf *itf;

    if (argc > 4) {
        return drv2605_shell_err_too_many_args(argv[1]);
    } else if (argc < 4) {
        return drv2605_shell_err_too_few_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], 0, 34, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    value = parse_ll_bounds(argv[3], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[3]);
    }

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    rc = drv2605_write8(itf, reg, value);
    if (rc) {
        console_printf("poke failed %d\n", rc);
    }else{
        console_printf("wrote: 0x%02X to 0x%02X\n", value, reg);
    }

    return 0;
}

static int
drv2605_shell_cmd_dump_all(int argc, char **argv, struct drv2605 *drv2605)
{
    int rc;
    uint8_t value;
    int i;
    struct sensor_itf *itf;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    for (i=0; i<=34; i++){
        rc = drv2605_read8(itf, i, &value);
        if (rc) {
            console_printf("dump failed %d\n", rc);
            goto err;
        }else{
            console_printf("reg 0x:%02X = 0x%02X\n", i, value);
        }
    }

    return 0;
err:
    return rc;
}

static int
drv2605_shell_load_cal(int argc, char **argv, struct drv2605_cal *cal)
{
    int rc;

    cal->brake_factor = parse_ll_bounds(argv[2], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal->loop_gain = parse_ll_bounds(argv[3], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal->lra_sample_time = parse_ll_bounds(argv[4], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal->lra_blanking_time = parse_ll_bounds(argv[5], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal->lra_idiss_time = parse_ll_bounds(argv[6], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal->auto_cal_time = parse_ll_bounds(argv[7], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal->lra_zc_det_time = parse_ll_bounds(argv[8], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    return 0;
}

static int
drv2605_shell_cmd_load_cal(int argc, char **argv, struct drv2605 *drv2605)
{
    int rc;

    if(argc == 2) {
        return drv2605_default_cal(&drv2605->cfg.cal);
    }else if (argc > 9) {
        return drv2605_shell_err_too_many_args(argv[1]);
    } else if (argc < 9) {
        return drv2605_shell_err_too_few_args(argv[1]);
    }

    rc = drv2605_shell_load_cal(argc, argv, &drv2605->cfg.cal);
    if (rc) {
        console_printf("load_cal failed %d\n", rc);
        goto err;
    }else{
        console_printf("load_cal succeeded\n");
    }

    return 0;
err:
    return rc;
}

static int
drv2605_shell_cmd_power_mode(int argc, char **argv, struct drv2605 *drv2605)
{
    int rc;
    enum drv2605_power_mode mode;
    struct sensor_itf *itf;

    if(strcmp(argv[2],"off") == 0) {
        mode = DRV2605_POWER_OFF;
    }else if(strcmp(argv[2],"standby") == 0) {
        mode = DRV2605_POWER_STANDBY;
    }else if(strcmp(argv[2],"active") == 0) {
        mode = DRV2605_POWER_ACTIVE;
    }else {
        return drv2605_shell_err_unknown_arg(argv[2]);
    }

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    rc = drv2605_set_power_mode(itf, mode);
    if (rc) {
        console_printf("power_mode failed %d\n", rc);
        goto err;
    }else{
        console_printf("power_mode succeeded\n");
    }

    return 0;
err:
    return rc;
}

static int
drv2605_shell_cmd_op_mode(int argc, char **argv, struct drv2605 *drv2605)
{
    int rc;

    if(strcmp(argv[2],"rom") == 0) {
        drv2605->cfg.op_mode = DRV2605_OP_ROM;
    }else if(strcmp(argv[2],"reset") == 0) {
        drv2605->cfg.op_mode = DRV2605_OP_RESET;
    }else if(strcmp(argv[2],"pwm") == 0) {
        drv2605->cfg.op_mode = DRV2605_OP_PWM;
    }else if(strcmp(argv[2],"analog") == 0) {
        drv2605->cfg.op_mode = DRV2605_OP_ANALOG;
    }else if(strcmp(argv[2],"rtp") == 0) {
        drv2605->cfg.op_mode = DRV2605_OP_RTP;
    }else if(strcmp(argv[2],"diag") == 0) {
        drv2605->cfg.op_mode = DRV2605_OP_DIAGNOSTIC;
    }else if(strcmp(argv[2],"cal") == 0) {
        drv2605->cfg.op_mode = DRV2605_OP_CALIBRATION;
    }else {
        return drv2605_shell_err_unknown_arg(argv[2]);
    }

    rc = drv2605_config(drv2605, &drv2605->cfg);
    if (rc) {
        console_printf("op_mode failed %d\n", rc);
        goto err;
    }else{
        console_printf("op_mode succeeded\n");
    }

    return 0;
err:
    return rc;
}

static int
drv2605_shell_cmd(int argc, char **argv)
{
    struct os_dev * dev;
    struct drv2605 * drv2605;

    dev = os_dev_open("drv2605_0", OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open drv2605_0 device\n");
        return ENODEV;
    }

    drv2605 = (struct drv2605 *)dev;

    if (argc == 1) {
        return drv2605_shell_help();
    }

    if (argc > 1 && strcmp(argv[1], "load_cal") == 0) {
        return drv2605_shell_cmd_load_cal(argc, argv, drv2605);
    }

    if (argc > 1 && strcmp(argv[1], "dump_cal") == 0) {
        return drv2605_shell_cmd_dump_cal(argc, argv, drv2605);
    }

    if (argc > 1 && strcmp(argv[1], "dump_all") == 0) {
        return drv2605_shell_cmd_dump_all(argc, argv, drv2605);
    }

    if (argc > 1 && strcmp(argv[1], "chip_id") == 0) {
        return drv2605_shell_cmd_get_chip_id(argc, argv, drv2605);
    }

    if (argc > 1 && strcmp(argv[1], "op_mode") == 0) {
        return drv2605_shell_cmd_op_mode(argc, argv, drv2605);
    }

    if (argc > 1 && strcmp(argv[1], "power_mode") == 0) {
        return drv2605_shell_cmd_power_mode(argc, argv, drv2605);
    }

    if (argc > 1 && strcmp(argv[1], "load") == 0) {
        return drv2605_shell_cmd_load_rom(argc, argv, drv2605);
    }

    if (argc > 1 && strcmp(argv[1], "trigger") == 0) {
        return drv2605_shell_cmd_trigger_rom(argc, argv, drv2605);
    }

    if (argc > 1 && strcmp(argv[1], "peek") == 0) {
        return drv2605_shell_cmd_peek(argc, argv, drv2605);
    }

    if (argc > 1 && strcmp(argv[1], "poke") == 0) {
        return drv2605_shell_cmd_poke(argc, argv, drv2605);
    }

    return drv2605_shell_err_unknown_arg(argv[1]);
}

int
drv2605_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&drv2605_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
