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
#include "hal/hal_gpio.h"
#include "bmp280/bmp280.h"
#include "bmp280_priv.h"
#include "parse/parse.h"

#if MYNEWT_VAL(BMP280_CLI)

static int bmp280_shell_cmd(int argc, char **argv);

static struct shell_cmd bmp280_shell_cmd_struct = {
    .sc_cmd = "bmp280",
    .sc_cmd_func = bmp280_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(BMP280_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(BMP280_SHELL_ITF_NUM),
    .si_cs_pin = MYNEWT_VAL(BMP280_SHELL_CSPIN),
    .si_addr = MYNEWT_VAL(BMP280_SHELL_ITF_ADDR)
};

static int
bmp280_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bmp280_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bmp280_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bmp280_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", bmp280_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tr    [n_samples]\n");
    console_printf("\tmode [0-sleep | 1/2-forced | 3-normal]\n");
    console_printf("\tiir [1-enabled | 0-disabled]\n");
    console_printf("\toversample [type 5-temperature | 6-pressure]\n"
                   "             [0-none | 1-x1 | 2-x2 | 3-x4 | 4-x8 | 5-x16]\n");
    console_printf("\treset\n");
    console_printf("\tchipid\n");
    console_printf("\tdump\n");

    return 0;
}

static int
bmp280_shell_cmd_read_chipid(int argc, char **argv)
{
    int rc;
    uint8_t chipid;

    rc = bmp280_get_chipid(&g_sensor_itf, &chipid);
    if (rc) {
        goto err;
    }

    console_printf("CHIP_ID:0x%02X\n", chipid);

    return 0;
err:
    return rc;
}

static int
bmp280_shell_cmd_reset(int argc, char **argv)
{
    return bmp280_reset(&g_sensor_itf);
}

static int
bmp280_shell_cmd_read(int argc, char **argv)
{
    int32_t temp;
    int32_t press;
    uint16_t samples = 1;
    uint16_t val;
    int rc;

    if (argc > 3) {
        return bmp280_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return bmp280_shell_err_invalid_arg(argv[2]);
        }
        samples = val;
    }

    while(samples--) {

        rc = bmp280_get_pressure(&g_sensor_itf, &press);
        if (rc) {
            console_printf("Read failed: %d\n", rc);
            return rc;
        }

        rc = bmp280_get_temperature(&g_sensor_itf, &temp);
        if (rc) {
            console_printf("Read failed: %d\n", rc);
            return rc;
        }

        console_printf("raw temperature: %d raw pressure: %d\n",
                       (int)temp, (int)press);
    }

    return 0;
}

static int
bmp280_shell_cmd_oversample(int argc, char **argv)
{
    uint8_t val;
    int rc;
    uint8_t oversample;
    uint32_t type;

    if (argc > 3) {
        return bmp280_shell_err_too_many_args(argv[1]);
    }

    /* Display the oversample */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 4, 8, &rc);
        if (rc) {
            return bmp280_shell_err_invalid_arg(argv[2]);
        }
        rc = bmp280_get_oversample(&g_sensor_itf, val, &oversample);
        if (rc) {
            goto err;
        }
        console_printf("%u\n", oversample);
    }

    /* Update the oversampling  */
    if (argc == 4) {
        val = parse_ll_bounds(argv[2], 4, 8, &rc);
        if (rc) {
            return bmp280_shell_err_invalid_arg(argv[2]);
        }

        type = val;

        val = parse_ll_bounds(argv[3], 0, 5, &rc);
        if (rc) {
            return bmp280_shell_err_invalid_arg(argv[3]);
        }

        oversample = val;

        rc = bmp280_set_oversample(&g_sensor_itf, type, oversample);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
bmp280_shell_cmd_mode(int argc, char **argv)
{
    uint8_t mode;
    uint8_t val;
    int rc;

    if (argc > 3) {
        return bmp280_shell_err_too_many_args(argv[1]);
    }

    if (argc == 2) {
        rc = bmp280_get_mode(&g_sensor_itf, &mode);
        if (rc) {
            goto err;
        }
        console_printf("mode: %u", mode);
    }

    /* Change mode */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, 3, &rc);
        if (rc) {
            return bmp280_shell_err_invalid_arg(argv[2]);
        }
        rc = bmp280_set_mode(&g_sensor_itf, val);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
bmp280_shell_cmd_iir(int argc, char **argv)
{
    uint8_t iir;
    uint8_t val;
    int rc;

    if (argc > 3) {
        return bmp280_shell_err_too_many_args(argv[1]);
    }

    /* Display if iir enabled */
    if (argc == 2) {
        rc = bmp280_get_iir(&g_sensor_itf, &iir);
        if (rc) {
            goto err;
        }
        console_printf("IIR: 0x%02X", iir);
    }

    /* Enable/disable iir*/
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, 1, &rc);
        if (rc) {
            return bmp280_shell_err_invalid_arg(argv[2]);
        }
        rc = bmp280_set_iir(&g_sensor_itf, val);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
bmp280_shell_cmd_dump(int argc, char **argv)
{
  uint8_t val;

  if (argc > 3) {
      return bmp280_shell_err_too_many_args(argv[1]);
  }

  /* Dump all the register values for debug purposes */
  val = 0;
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_T1, &val, 1));
  console_printf("0x%02X (DIG_T1): 0x%02X\n", BMP280_REG_ADDR_DIG_T1, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_T2, &val, 1));
  console_printf("0x%02X (DIG_T2):  0x%02X\n", BMP280_REG_ADDR_DIG_T2, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_T3, &val, 1));
  console_printf("0x%02X (DIG_T3):   0x%02X\n", BMP280_REG_ADDR_DIG_T3, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_P1, &val, 1));
  console_printf("0x%02X (DIG_P1): 0x%02X\n", BMP280_REG_ADDR_DIG_P1, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_P2, &val, 1));
  console_printf("0x%02X (DIG_P2):  0x%02X\n", BMP280_REG_ADDR_DIG_P2, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_P3, &val, 1));
  console_printf("0x%02X (DIG_P3):   0x%02X\n", BMP280_REG_ADDR_DIG_P3, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_P4, &val, 1));
  console_printf("0x%02X (DIG_P4): 0x%02X\n", BMP280_REG_ADDR_DIG_P4, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_P5, &val, 1));
  console_printf("0x%02X (DIG_P5):  0x%02X\n", BMP280_REG_ADDR_DIG_P5, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_P6, &val, 1));
  console_printf("0x%02X (DIG_P6):   0x%02X\n", BMP280_REG_ADDR_DIG_P6, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_P7, &val, 1));
  console_printf("0x%02X (DIG_P7): 0x%02X\n", BMP280_REG_ADDR_DIG_P7, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_P8, &val, 1));
  console_printf("0x%02X (DIG_P8):  0x%02X\n", BMP280_REG_ADDR_DIG_P8, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_DIG_P9, &val, 1));
  console_printf("0x%02X (DIG_P9):   0x%02X\n", BMP280_REG_ADDR_DIG_P9, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_CHIPID, &val, 1));
  console_printf("0x%02X (CHIPID):   0x%02X\n", BMP280_REG_ADDR_CHIPID, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_VERSION, &val, 1));
  console_printf("0x%02X (VER):   0x%02X\n", BMP280_REG_ADDR_VERSION, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_STATUS, &val, 1));
  console_printf("0x%02X (STATUS):   0x%02X\n", BMP280_REG_ADDR_STATUS, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_CTRL_MEAS, &val, 1));
  console_printf("0x%02X (CTRL_MEAS):   0x%02X\n", BMP280_REG_ADDR_CTRL_MEAS, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_CONFIG, &val, 1));
  console_printf("0x%02X (CONFIG):   0x%02X\n", BMP280_REG_ADDR_CONFIG, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_PRESS_MSB, &val, 1));
  console_printf("0x%02X (PRESS MSB):   0x%02X\n", BMP280_REG_ADDR_PRESS_MSB, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_PRESS_LSB, &val, 1));
  console_printf("0x%02X (PRESS LSB):   0x%02X\n", BMP280_REG_ADDR_PRESS_LSB, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_PRESS_XLSB, &val, 1));
  console_printf("0x%02X (PRESS XLSB):   0x%02X\n", BMP280_REG_ADDR_PRESS_XLSB, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_TEMP_XLSB, &val, 1));
  console_printf("0x%02X (TEMP MSB):   0x%02X\n", BMP280_REG_ADDR_TEMP_MSB, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_TEMP_LSB, &val, 1));
  console_printf("0x%02X (TEMP LSB):   0x%02X\n", BMP280_REG_ADDR_TEMP_LSB, val);
  assert(0 == bmp280_readlen(&g_sensor_itf, BMP280_REG_ADDR_TEMP_XLSB, &val, 1));
  console_printf("0x%02X (TEMP XLSB):   0x%02X\n", BMP280_REG_ADDR_TEMP_XLSB, val);

  return 0;
}

static int
bmp280_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return bmp280_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return bmp280_shell_cmd_read(argc, argv);
    }

    /* Mode command */
    if (argc > 1 && strcmp(argv[1], "mode") == 0) {
        return bmp280_shell_cmd_mode(argc, argv);
    }

    /* IIR */
    if (argc > 1 && strcmp(argv[1], "iir") == 0) {
        return bmp280_shell_cmd_iir(argc, argv);
    }

    /* Oversample */
    if (argc > 1 && strcmp(argv[1], "oversample") == 0) {
        return bmp280_shell_cmd_oversample(argc, argv);
    }

    /* Reset */
    if (argc > 1 && strcmp(argv[1], "reset") == 0) {
        return bmp280_shell_cmd_reset(argc, argv);
    }

    /* Chip ID */
    if (argc > 1 && strcmp(argv[1], "chipid") == 0) {
        return bmp280_shell_cmd_read_chipid(argc, argv);
    }

    /* Dump */
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return bmp280_shell_cmd_dump(argc, argv);
    }

    return bmp280_shell_err_unknown_arg(argv[1]);
}

int
bmp280_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&bmp280_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
