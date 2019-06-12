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
#include "hal/hal_gpio.h"
#include "bmp280/bmp280.h"
#include "bmp280_priv.h"
#include "bsp/bsp.h"

#if MYNEWT_VAL(BMP280_CLI)

#include "shell/shell.h"
#include "parse/parse.h"

static int bmp280_shell_cmd(int argc, char **argv);

static struct shell_cmd bmp280_shell_cmd_struct = {
    .sc_cmd = "bmp280",
    .sc_cmd_func = bmp280_shell_cmd
};

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static struct sensor_itf g_sensor_itf;
#else
static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(BMP280_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(BMP280_SHELL_ITF_NUM),
    .si_cs_pin = MYNEWT_VAL(BMP280_SHELL_CSPIN),
    .si_addr = MYNEWT_VAL(BMP280_SHELL_ITF_ADDR)
};
#endif

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

    if (argc > 4) {
        return bmp280_shell_err_too_many_args(argv[1]);
    }

    /* Display the oversampling */
    if (argc == 2) {
        rc = bmp280_get_oversample(&g_sensor_itf,
                                   SENSOR_TYPE_AMBIENT_TEMPERATURE, &oversample);
        if (rc == 0) {
            if (oversample == 0) {
                console_printf("Temperature measurement disabled\n");
            } else {
                console_printf("Temperature oversampling %u (x%u)\n",
                               oversample, 1U << (oversample - 1));
            }
        } else {
            console_printf("Error reading temperature oversampling %d\n", rc);
        }
        rc = bmp280_get_oversample(&g_sensor_itf, SENSOR_TYPE_PRESSURE,
                                   &oversample);
        if (rc == 0) {
            if (oversample == 0) {
                console_printf("Pressure measurement disabled\n");
            } else {
                console_printf("Pressure oversampling %u (x%u)\n",
                               oversample, 1U << (oversample - 1));
            }
        } else {
            console_printf("Error reading pressure oversampling %d\n", rc);
        }
    } else if (argc == 3) {
        /* Display the oversample */
        val = (uint8_t)parse_ll_bounds(argv[2], 5, 6, &rc);
        if (rc) {
            return bmp280_shell_err_invalid_arg(argv[2]);
        }
        rc = bmp280_get_oversample(&g_sensor_itf, 1U << val, &oversample);
        if (rc) {
            goto err;
        }
        console_printf("%u\n", oversample);
    }

    /* Update the oversampling  */
    if (argc == 4) {
        val = (uint8_t)parse_ll_bounds(argv[2], 5, 6, &rc);
        if (rc) {
            return bmp280_shell_err_invalid_arg(argv[2]);
        }

        type = 1U << val;

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

static void
bmp280_shell_dump_reg(const char *name, uint8_t addr)
{
    uint8_t val = 0;
    int rc;

    rc = bmp280_readlen(&g_sensor_itf, addr, &val, 1);
    if (rc == 0) {
        console_printf("0x%02X (%s): 0x%02X\n", addr, name, val);
    } else {
        console_printf("0x%02X (%s): failed (%d)\n", addr, name, rc);
    }
}

#define DUMP_REG(name) bmp280_shell_dump_reg(#name, BMP280_REG_ADDR_ ## name)

static int
bmp280_shell_cmd_dump(int argc, char **argv)
{
  if (argc > 3) {
      return bmp280_shell_err_too_many_args(argv[1]);
  }

  /* Dump all the register values for debug purposes */
  DUMP_REG(DIG_T1);
  DUMP_REG(DIG_T2);
  DUMP_REG(DIG_T3);
  DUMP_REG(DIG_P1);
  DUMP_REG(DIG_P2);
  DUMP_REG(DIG_P3);
  DUMP_REG(DIG_P4);
  DUMP_REG(DIG_P5);
  DUMP_REG(DIG_P6);
  DUMP_REG(DIG_P7);
  DUMP_REG(DIG_P8);
  DUMP_REG(DIG_P9);
  DUMP_REG(CHIPID);
  DUMP_REG(VERSION);
  DUMP_REG(STATUS);
  DUMP_REG(CTRL_MEAS);
  DUMP_REG(CONFIG);
  DUMP_REG(PRESS_MSB);
  DUMP_REG(PRESS_LSB);
  DUMP_REG(PRESS_XLSB);
  DUMP_REG(TEMP_XLSB);
  DUMP_REG(TEMP_LSB);
  DUMP_REG(TEMP_XLSB);

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

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)

#if MYNEWT_VAL(BMP280_SHELL_ITF_TYPE) == SENSOR_ITF_I2C
struct bmp280 bmp280_raw;

static const struct bus_i2c_node_cfg bmp280_raw_cfg = {
    .node_cfg = {
        .bus_name = MYNEWT_VAL(BMP280_SHELL_ITF_BUS),
    },
    .addr = MYNEWT_VAL(BMP280_SHELL_ITF_ADDR),
    .freq = 400,
};
#elif MYNEWT_VAL(BMP280_SHELL_ITF_TYPE) == SENSOR_ITF_SPI
struct bmp280 bmp280_raw = {
    .node_is_spi = true,
};

static const struct bus_spi_node_cfg bmp280_raw_cfg = {
    .node_cfg = {
        .bus_name = MYNEWT_VAL(BMP280_SHELL_ITF_BUS),
    },
    .pin_cs = MYNEWT_VAL(BMP280_SHELL_CSPIN),
    .data_order = BUS_SPI_DATA_ORDER_MSB,
    .mode = BUS_SPI_MODE_0,
    .freq = 4000,
};
#endif

#endif

int
bmp280_shell_init(void)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct os_dev *dev = NULL;

    g_sensor_itf.si_dev = (struct os_dev *)&bmp280_raw;
#if MYNEWT_VAL(BMP280_SHELL_ITF_TYPE) == SENSOR_ITF_I2C
    rc = bus_i2c_node_create("bmp280_raw", &bmp280_raw.i2c_node,
                             &bmp280_raw_cfg, &g_sensor_itf);
#elif MYNEWT_VAL(BMP280_SHELL_ITF_TYPE) == SENSOR_ITF_SPI
    rc = bus_spi_node_create("bmp280_raw", &bmp280_raw.spi_node,
                             &bmp280_raw_cfg, &g_sensor_itf);
#endif
    if (rc == 0) {
        dev = os_dev_open("bmp280_raw", 0, NULL);
    }
    if (rc != 0 || dev == NULL) {
        console_printf("Failed to create bmp280_raw device\n");
    }
#endif
    rc = shell_cmd_register(&bmp280_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
