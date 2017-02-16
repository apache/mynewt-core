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
#include "bno055_priv.h"
#include "bno055/bno055.h"
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/mag.h"
#include "sensor/quat.h"
#include "sensor/euler.h"
#include "hal/hal_i2c.h"

#if MYNEWT_VAL(BNO055_CLI)
extern uint8_t g_bno055_mode;

static int bno055_shell_cmd(int argc, char **argv);

static struct shell_cmd bno055_shell_cmd_struct = {
    .sc_cmd = "bno055",
    .sc_cmd_func = bno055_shell_cmd
};

static int
bno055_shell_stol(char *param_val, long min, long max, long *output)
{
    char *endptr;
    long lval;

    lval = strtol(param_val, &endptr, 10); /* Base 10 */
    if (param_val != '\0' && *endptr == '\0' &&
        lval >= min && lval <= max) {
            *output = lval;
    } else {
        return EINVAL;
    }

    return 0;
}

static int
bno055_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bno055_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bno055_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bno055_shell_help(void)
{
    console_printf("%s cmd  [flags...]\n", bno055_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tr     [n_samples] [ 0-acc          | 1 -mag       | 2 -gyro    | 4 -temp   |\n"
                   "\t                    9-quat         | 26-linearacc | 27-gravity | 28-euler  ]\n\n");
    console_printf("\tmode  [0-config   | 1-acc          | 2 -mag       | 3 -gyro    | 4 -accmag |\n"
                   "\t       5-accgyro  | 6-maggyro      | 7 -amg       | 8 -imuplus | 9 -compass|\n"
                   "\t       9-m4g      |11-NDOF_FMC_OFF | 12-NDOF  ]\n");
    console_printf("\tchip_id\n");
    console_printf("\trev\n");
    console_printf("\treset\n");
    console_printf("\tpmode [0-normal   | 1-lowpower     | 2-suspend]\n");
    console_printf("\tdumpreg [addr]\n");

    return 0;
}

static int
bno055_shell_cmd_get_chip_id(int argc, char **argv)
{
    uint8_t id;
    int rc;

    if (argc > 3) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Display the chip id */
    if (argc == 2) {
        rc = bno055_get_chip_id(&id);
        if (rc) {
            console_printf("Read failed %d", rc);
        }
        console_printf("0x%02X\n", id);
    }

    return 0;
}

static int
bno055_shell_cmd_get_rev_info(int argc, char **argv)
{
    int rc;
    struct bno055_rev_info ri;

    if (argc > 3) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Display the rev ids */
    if (argc == 2) {
        rc = bno055_get_rev_info(&ri);
        if (rc) {
            console_printf("Read failed %d", rc);
        }
        console_printf("accel_rev:0x%02X\nmag_rev:0x%02X\ngyro_rev:0x%02X\n"
                       "sw_rev:0x%02X\nbl_rev:0x%02X\n", ri.bri_accel_rev,
                       ri.bri_mag_rev, ri.bri_gyro_rev, ri.bri_sw_rev,
                       ri.bri_bl_rev);
    }

    return 0;
}

static int
bno055_shell_cmd_read(int argc, char **argv)
{
    uint16_t samples = 1;
    long val;
    int rc;
    void *databuf;
    struct sensor_quat_data *sqd;
    struct sensor_euler_data *sed;
    struct sensor_accel_data *sad;
    int type;
    char tmpstr[13];

    type = 0;
    if (argc > 4) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Since this is the biggest struct, malloc space for it */
    databuf = malloc(sizeof(*sqd));


    /* Check if more than one sample requested */
    if (argc == 4) {
        if (bno055_shell_stol(argv[2], 1, UINT16_MAX, &val)) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }
        samples = (uint16_t)val;

        if (bno055_shell_stol(argv[3], 0, UINT16_MAX, &val)) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }
        type = (int)(1 << val);
    }

    while(samples--) {

        if (type == SENSOR_TYPE_ROTATION_VECTOR) {
            rc = bno055_get_quat_data(databuf);
            if (rc) {
                console_printf("Read failed: %d\n", rc);
                goto err;
            }
            sqd = databuf;

            console_printf("x:%s ", sensor_ftostr(sqd->sqd_x, tmpstr, 13));
            console_printf("y:%s ", sensor_ftostr(sqd->sqd_y, tmpstr, 13));
            console_printf("z:%s ", sensor_ftostr(sqd->sqd_z, tmpstr, 13));
            console_printf("w:%s\n", sensor_ftostr(sqd->sqd_w, tmpstr, 13));

        } else if (type == SENSOR_TYPE_EULER) {
            rc = bno055_get_vector_data(databuf, type);
            if (rc) {
                console_printf("Read failed: %d\n", rc);
                goto err;
            }
            sed = databuf;

            console_printf("h:%s ", sensor_ftostr(sed->sed_h, tmpstr, 13));
            console_printf("r:%s ", sensor_ftostr(sed->sed_r, tmpstr, 13));
            console_printf("p:%s\n", sensor_ftostr(sed->sed_p, tmpstr, 13));

        } else {
            rc = bno055_get_vector_data(databuf, type);
            if (rc) {
                console_printf("Read failed: %d\n", rc);
                goto err;
            }
            sad = databuf;

            console_printf("x:%s ", sensor_ftostr(sad->sad_x, tmpstr, 13));
            console_printf("y:%s ", sensor_ftostr(sad->sad_y, tmpstr, 13));
            console_printf("z:%s\n", sensor_ftostr(sad->sad_z, tmpstr, 13));
        }
    }

    free(databuf);

    return 0;
err:
    return rc;
}

static int
bno055_shell_cmd_opr_mode(int argc, char **argv)
{
    long val;
    int rc;

    if (argc > 3) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Display the mode */
    if (argc == 2) {
        val = bno055_get_opr_mode();
        console_printf("%u\n", (unsigned int)val);
    }

    /* Update the mode */
    if (argc == 3) {
        if (bno055_shell_stol(argv[2], 0, 16, &val)) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }
        /* Make sure mode is valid */
        if (val > BNO055_OPERATION_MODE_NDOF) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }

        rc = bno055_set_opr_mode(val);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
bno055_shell_cmd_pwr_mode(int argc, char **argv)
{
    long val;
    int rc;

    if (argc > 3) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Display the mode */
    if (argc == 2) {
        val = bno055_get_pwr_mode();
        console_printf("%u\n", (unsigned int)val);
    }

    /* Update the mode */
    if (argc == 3) {
        if (bno055_shell_stol(argv[2], 0, 16, &val)) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }
        /* Make sure mode is valid */
        if (val > BNO055_POWER_MODE_SUSPEND) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }

        rc = bno055_set_pwr_mode(val);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
bno055_shell_cmd_dumpreg(int argc, char **argv)
{
    long addr;
    uint8_t val;
    int rc;

    if (bno055_shell_stol(argv[2], 0, UINT8_MAX, &addr)) {
        return bno055_shell_err_invalid_arg(argv[2]);
    }
    rc = bno055_read8((uint8_t)addr, &val);
    if (rc) {
        goto err;
    }
    console_printf("0x%02X (ADDR): 0x%02X", (uint8_t)addr, val);

    return 0;
err:
    return rc;
}

static int
shell_i2cscan_cmd(int argc, char **argv)
{
    uint8_t addr;
    int32_t timeout;
    uint8_t dev_count = 0;
    long i2cnum;
    int rc;

    timeout = OS_TICKS_PER_SEC / 10;

    if (bno055_shell_stol(argv[2], 0, 0xf, &i2cnum)) {
        return bno055_shell_err_invalid_arg(argv[2]);
    }

    console_printf("Scanning I2C bus %u\n"
                   "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n"
                   "00:          ", (uint8_t)i2cnum);


    /* Scan all valid I2C addresses (0x08..0x77) */
    for (addr = 0x08; addr < 0x78; addr++) {
        rc = hal_i2c_master_probe((uint8_t)i2cnum, addr, timeout);
        /* Print addr header every 16 bytes */
        if (!(addr % 16)) {
            console_printf("\n%02x: ", addr);
        }
        /* Display the addr if a response was received */
        if (!rc) {
            console_printf("%02x ", addr);
            dev_count++;
        } else {
            console_printf("-- ");
        }
        os_time_delay(OS_TICKS_PER_SEC/1000 * 20);
    }
    console_printf("\nFound %u devices on I2C bus %u\n", dev_count, (uint8_t)i2cnum);

    return 0;
}

static int
bno055_shell_cmd_reset(int argc, char **argv)
{
    int rc;
    /* Reset sensor */
    rc = bno055_write8(BNO055_SYS_TRIGGER_ADDR, BNO055_SYS_TRIGGER_RST_SYS);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

static int
bno055_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return bno055_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return bno055_shell_cmd_read(argc, argv);
    }

    /* Mode command */
    if (argc > 1 && strcmp(argv[1], "mode") == 0) {
        return bno055_shell_cmd_opr_mode(argc, argv);
    }

    /* Chip ID command */
    if (argc > 1 && strcmp(argv[1], "chip_id") == 0) {
        return bno055_shell_cmd_get_chip_id(argc, argv);
    }

    /* Rev command */
    if (argc > 1 && strcmp(argv[1], "rev") == 0) {
        return bno055_shell_cmd_get_rev_info(argc, argv);
    }
    /* Reset command */
    if (argc > 1 && strcmp(argv[1], "reset") == 0) {
        return bno055_shell_cmd_reset(argc, argv);
    }

    /* Power mode command */
    if (argc > 1 && strcmp(argv[1], "pmode") == 0) {
        return bno055_shell_cmd_pwr_mode(argc, argv);
    }

    /* Dump Registers command */
    if (argc > 1 && strcmp(argv[1], "dumpreg") == 0) {
        return bno055_shell_cmd_dumpreg(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "i2cscan") == 0) {
        return shell_i2cscan_cmd(argc, argv);
    }
    return bno055_shell_err_unknown_arg(argv[1]);
}

int
bno055_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&bno055_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
