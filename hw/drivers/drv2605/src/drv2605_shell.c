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
#include "sysinit/sysinit.h"
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

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(DRV2605_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(DRV2605_SHELL_ITF_NUM),
    .si_addr = MYNEWT_VAL(DRV2605_SHELL_ITF_ADDR),
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
    console_printf("\treset\n");
    console_printf("\tdefaults\n");
    console_printf("\tdiag\n");
    console_printf("\tautocal [brake_factor loop_gain lra_sample_time lra_blanking_time lra_idiss_time auto_cal_time lra_zc_det_time]\n");
    console_printf("\tload [up to 8 uint8_t]\n");
    console_printf("\ttrigger\n");
    console_printf("\tpeek [reg]\n");
    console_printf("\tpoke [reg value]\n");
    console_printf("\tdump_cal\n");
    console_printf("\tdump_all\n");

    return 0;
}

static int
drv2605_shell_cmd_load_waveform(int argc, char **argv)
{
    int rc;
    uint8_t waveform[8] = {0};

    if (argc > 10) {
        return drv2605_shell_err_too_many_args(argv[1]);
    } else if (argc < 3) {
        return drv2605_shell_err_too_few_args(argv[1]);
    }

    uint8_t len = argc-2;
    uint8_t i;

    for (i = 0; i<len;i++) {
        waveform[i] = parse_ll_bounds(argv[i+2], 0, 255, &rc);
        if (rc != 0) {
            goto err;
        }
    }

    rc = drv2605_load(&g_sensor_itf, waveform, len);
    if (rc) {
        console_printf("Load failed %d\n", rc);
    }else{
        console_printf("Load succeeded\n");
    }

    return 0;
err:
    return rc;
}

static int
drv2605_shell_cmd_trigger_waveform(int argc, char **argv)
{
    int rc;

    rc =  drv2605_internal_trigger(&g_sensor_itf);
    if (rc) {
        console_printf("Trigger failed %d\n", rc);
    }else{
        console_printf("Trigger succeeded\n");
    }

    return rc;
}

static int
drv2605_shell_cmd_get_chip_id(int argc, char **argv)
{
    uint8_t id;
    int rc;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    /* Display the chip id */
    if (argc == 2) {
        rc = drv2605_get_chip_id(&g_sensor_itf, &id);
        if (rc) {
            console_printf("Read failed %d\n", rc);
        }else{
            console_printf("0x%02X\n", id);
        }
    }

    return 0;
}

static int
drv2605_shell_cmd_diag(int argc, char **argv)
{
    int rc;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    if (argc == 2) {
        rc = drv2605_diagnostics(&g_sensor_itf);
        if (rc) {
            console_printf("Diagnostic failed %d\n", rc);
        }else{
            console_printf("Diagnostic Complete\n");
        }
    }

    return 0;
}

static int
drv2605_shell_cmd_reset(int argc, char **argv)
{
    int rc;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    if (argc == 2) {
        rc = drv2605_reset(&g_sensor_itf);
        if (rc) {
            console_printf("Reset failed %d\n", rc);
        }else{
            console_printf("Reset Complete\n");
        }
    }

    return 0;
}

static int
drv2605_shell_cmd_dump_cal(int argc, char **argv)
{
    int rc;
    uint8_t tmp[3];

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    if (argc == 2) {
        rc = drv2605_readlen(&g_sensor_itf, DRV2605_AUTO_CALIBRATION_COMPENSATION_RESULT_ADDR, &tmp[0], sizeof(tmp));
        if (rc) {
            console_printf("Read failed %d\n", rc);
            goto err;
        }
        console_printf("\nDRV2605_CALIBRATED_COMP: 0x%02X\nDRV2605_CALIBRATED_BEMF: 0x%02X\nDRV2605_CALIBRATED_BEMF_GAIN: %0d\n", tmp[0], tmp[1], tmp[2] & 0x03);
    }

    return 0;
err:
    return rc;
}

static int
drv2605_shell_cmd_peek(int argc, char **argv)
{
    int rc;
    uint8_t value;
    uint8_t reg;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    } else if (argc < 3) {
        return drv2605_shell_err_too_few_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], 0, 34, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    rc = drv2605_read8(&g_sensor_itf, reg, &value);
    if (rc) {
        console_printf("peek read failed %d\n", rc);
    }else{
        console_printf("Value: 0x%02X\n", value);
    }

    return 0;
}

static int
drv2605_shell_cmd_poke(int argc, char **argv)
{
    int rc;
    uint8_t reg;
    uint8_t value;

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

    rc = drv2605_write8(&g_sensor_itf, reg, value);
    if (rc) {
        console_printf("Write8 failed %d\n", rc);
    }else{
        console_printf("Wrote: 0x%02X to 0x%02X\n", value, reg);
    }

    return 0;
}

static int
drv2605_shell_cmd_dump_all(int argc, char **argv)
{
    int rc;
    uint8_t value;
    int i;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    for (i=0; i<=34; i++){
        rc = drv2605_read8(&g_sensor_itf, i, &value);
        if (rc) {
            console_printf("dump read failed %d\n", rc);
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
drv2605_shell_cmd_autocal(int argc, char **argv)
{
    int rc;
    struct drv2605_cal cal;

    if (argc > 9) {
        return drv2605_shell_err_too_many_args(argv[1]);
    } else if (argc < 9) {
        return drv2605_shell_err_too_few_args(argv[1]);
    }

    cal.brake_factor = parse_ll_bounds(argv[2], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal.loop_gain = parse_ll_bounds(argv[3], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal.lra_sample_time = parse_ll_bounds(argv[4], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal.lra_blanking_time = parse_ll_bounds(argv[5], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal.lra_idiss_time = parse_ll_bounds(argv[6], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal.auto_cal_time = parse_ll_bounds(argv[7], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    cal.lra_zc_det_time = parse_ll_bounds(argv[8], 0, 255, &rc);
    if (rc != 0) {
        return drv2605_shell_err_invalid_arg(argv[2]);
    }

    rc = drv2605_auto_calibrate(&g_sensor_itf, &cal);
    if (rc) {
        console_printf("Read failed %d\n", rc);
    }else{
        console_printf("Autocal Complete\n");
        drv2605_shell_cmd_dump_cal(2, NULL);
    }

    return 0;
}

static int
drv2605_shell_cmd_defaults(int argc, char **argv)
{
    int rc;

    if (argc > 3) {
        return drv2605_shell_err_too_many_args(argv[1]);
    }

    if (argc == 2) {
        rc = drv2605_send_defaults(&g_sensor_itf);
        if (rc) {
            console_printf("Read failed %d\n", rc);
        }else{
            console_printf("Calibration values written\n");
            drv2605_shell_cmd_dump_cal(2, NULL);
        }
    }

    return 0;
}

static int
drv2605_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return drv2605_shell_help();
    }

    /* Cal command */
    if (argc > 1 && strcmp(argv[1], "defaults") == 0) {
        return drv2605_shell_cmd_defaults(argc, argv);
    }

    /* Autocal command */
    if (argc > 1 && strcmp(argv[1], "autocal") == 0) {
        return drv2605_shell_cmd_autocal(argc, argv);
    }

    /* Dump command */
    if (argc > 1 && strcmp(argv[1], "dump_cal") == 0) {
        return drv2605_shell_cmd_dump_cal(argc, argv);
    }

    /* Dump command */
    if (argc > 1 && strcmp(argv[1], "dump_all") == 0) {
        return drv2605_shell_cmd_dump_all(argc, argv);
    }

    /* Chip ID command */
    if (argc > 1 && strcmp(argv[1], "chip_id") == 0) {
        return drv2605_shell_cmd_get_chip_id(argc, argv);
    }

    /* Reset command */
    if (argc > 1 && strcmp(argv[1], "reset") == 0) {
        return drv2605_shell_cmd_reset(argc, argv);
    }

    /* Reset command */
    if (argc > 1 && strcmp(argv[1], "diag") == 0) {
        return drv2605_shell_cmd_diag(argc, argv);
    }

    /* Load waveform */
    if (argc > 1 && strcmp(argv[1], "load") == 0) {
        return drv2605_shell_cmd_load_waveform(argc, argv);
    }

    /* Trigger stored waveform */
    if (argc > 1 && strcmp(argv[1], "trigger") == 0) {
        return drv2605_shell_cmd_trigger_waveform(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "peek") == 0) {
        return drv2605_shell_cmd_peek(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "poke") == 0) {
        return drv2605_shell_cmd_poke(argc, argv);
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
