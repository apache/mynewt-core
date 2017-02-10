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
        console_printf("%x\n", id ? 16u : 1u);
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
        console_printf("accel_rev:%x\nmag_rev:%x\ngyro_rev:%x\n"
                       "sw_rev:%x\nbl_rev:%x\n", ri.bri_accel_rev,
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

        if (bno055_shell_stol(argv[2], 1, UINT16_MAX, &val)) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }
        type = (uint16_t)(1 << val);
    }

    while(samples--) {

        if (type == SENSOR_TYPE_ROTATION_VECTOR) {
            rc = bno055_get_quat_data(databuf);
            if (rc) {
                console_printf("Read failed: %d\n", rc);
                goto err;
            }
            sqd = databuf;
            console_printf("x:%u y:%u z:%u w:%u\n", (unsigned int)sqd->sqd_x,
                          (unsigned int)sqd->sqd_y, (unsigned int)sqd->sqd_z,
                          (unsigned int)sqd->sqd_w);
        } else if (type == SENSOR_TYPE_EULER) {
            rc = bno055_get_vector_data(databuf, type);
            if (rc) {
                console_printf("Read failed: %d\n", rc);
            }
            sed = databuf;
            console_printf("h:%u r:%u p:%u\n", (unsigned int)sed->sed_h,
                          (unsigned int)sed->sed_r, (unsigned int)sed->sed_p);
        } else {
            rc = bno055_get_vector_data(databuf, type);
            if (rc) {
                console_printf("Read failed: %d\n", rc);
            }
            sad = databuf;
            console_printf("x:%u y:%u z:%u\n", (unsigned int)sad->sad_x,
                          (unsigned int)sad->sad_y, (unsigned int)sad->sad_z);
        }
    }

    return 0;
err:
    return rc;
}

static int
bno055_shell_cmd_mode(int argc, char **argv)
{
    long val;
    int rc;

    if (argc > 3) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Display the mode */
    if (argc == 2) {
        val = bno055_get_mode();
        console_printf("%u\n", val ? 16u : 1u);
    }

    /* Update the mode */
    if (argc == 3) {
        if (bno055_shell_stol(argv[2], 1, 16, &val)) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }
        /* Make sure mode is */
        if (val > BNO055_OPERATION_MODE_NDOF) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }

        rc = bno055_set_mode(val);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

#if 0
static int
bno055_shell_cmd_int(int argc, char **argv)
{
    int rc;
    int pin;
    long val;
    uint8_t rate;
    uint16_t lower;
    uint16_t upper;

    if (argc > 6) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    if (argc == 2) {
        console_printf("ToDo: Display int details\n");
        return 0;
    }

    /* Enable the interrupt */
    if (argc == 3 && strcmp(argv[2], "on") == 0) {
        return bno055_enable_interrupt(1);
    }

    /* Disable the interrupt */
    if (argc == 3 && strcmp(argv[2], "off") == 0) {
        return bno055_enable_interrupt(0);
    }

    /* Clear the interrupt on 'clr' */
    if (argc == 3 && strcmp(argv[2], "clr") == 0) {
        return bno055_clear_interrupt();
    }

    /* Configure the interrupt on 'set' */
    if (argc == 6 && strcmp(argv[2], "set") == 0) {
        /* Get rate */
        if (bno055_shell_stol(argv[3], 0, 15, &val)) {
            return bno055_shell_err_invalid_arg(argv[3]);
        }
        rate = (uint8_t)val;
        /* Get lower threshold */
        if (bno055_shell_stol(argv[4], 0, UINT16_MAX, &val)) {
            return bno055_shell_err_invalid_arg(argv[4]);
        }
        lower = (uint16_t)val;
        /* Get upper threshold */
        if (bno055_shell_stol(argv[5], 0, UINT16_MAX, &val)) {
            return bno055_shell_err_invalid_arg(argv[5]);
        }
        upper = (uint16_t)val;
        /* Set the values */
        rc = bno055_setup_interrupt(rate, lower, upper);
        console_printf("Configured interrupt as:\n");
        console_printf("\trate: %u\n", rate);
        console_printf("\tlower: %u\n", lower);
        console_printf("\tupper: %u\n", upper);
        return rc;
    }

    /* Setup INT pin on 'pin' */
    if (argc == 4 && strcmp(argv[2], "pin") == 0) {
        /* Get the pin number */
        if (bno055_shell_stol(argv[3], 0, 0xFF, &val)) {
            return bno055_shell_err_invalid_arg(argv[3]);
        }
        pin = (int)val;
        /* INT is open drain, pullup is required */
        rc = hal_gpio_init_in(pin, HAL_GPIO_PULL_UP);
        assert(rc == 0);
        console_printf("Set pin \"%d\" to INPUT with pull up enabled\n", pin);
        return 0;
    }

    /* Unknown command */
    return bno055_shell_err_invalid_arg(argv[2]);
}

static int
bno055_shell_cmd_dump(int argc, char **argv)
{
  uint8_t val;

  if (argc > 3) {
      return bno055_shell_err_too_many_args(argv[1]);
  }

  val = 0;
  assert(0 == bno055_read8(BNO055_COMMAND_BIT | BNO055_REGISTER_CONTROL, &val));
  console_printf("0x%02X (CONTROL): 0x%02X\n", BNO055_REGISTER_CONTROL, val);
  assert(0 == bno055_read8(BNO055_COMMAND_BIT | BNO055_REGISTER_TIMING, &val));
  console_printf("0x%02X (TIMING):  0x%02X\n", BNO055_REGISTER_TIMING, val);
  assert(0 == bno055_read8(BNO055_COMMAND_BIT | BNO055_REGISTER_THRESHHOLDL_LOW, &val));
  console_printf("0x%02X (THRLL):   0x%02X\n", BNO055_REGISTER_THRESHHOLDL_LOW, val);
  assert(0 == bno055_read8(BNO055_COMMAND_BIT | BNO055_REGISTER_THRESHHOLDL_HIGH, &val));
  console_printf("0x%02X (THRLH):   0x%02X\n", BNO055_REGISTER_THRESHHOLDL_HIGH, val);
  assert(0 == bno055_read8(BNO055_COMMAND_BIT | BNO055_REGISTER_THRESHHOLDH_LOW, &val));
  console_printf("0x%02X (THRHL):   0x%02X\n", BNO055_REGISTER_THRESHHOLDH_LOW, val);
  assert(0 == bno055_read8(BNO055_COMMAND_BIT | BNO055_REGISTER_THRESHHOLDH_HIGH, &val));
  console_printf("0x%02X (THRHH):   0x%02X\n", BNO055_REGISTER_THRESHHOLDH_HIGH, val);
  assert(0 == bno055_read8(BNO055_COMMAND_BIT | BNO055_REGISTER_INTERRUPT, &val));
  console_printf("0x%02X (INTER):   0x%02X\n", BNO055_REGISTER_INTERRUPT, val);
  assert(0 == bno055_read8(BNO055_COMMAND_BIT | BNO055_REGISTER_ID, &val));
  console_printf("0x%02X (ID):      0x%02X\n", BNO055_REGISTER_ID, val);

  return 0;
}
#endif

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
        return bno055_shell_cmd_mode(argc, argv);
    }

    /* Chip ID command */
    if (argc > 1 && strcmp(argv[1], "chip_id") == 0) {
        return bno055_shell_cmd_get_chip_id(argc, argv);
    }

    /* Rev command */
    if (argc > 1 && strcmp(argv[1], "rev") == 0) {
        return bno055_shell_cmd_get_rev_info(argc, argv);
    }
#if 0
    /* Reset command */
    if (argc > 1 && strcmp(argv[1], "reset") == 0) {
        return bno055_shell_cmd_reset(argc, argv);
    }

    /* Power mode command */
    if (argc > 1 && strcmp(argv[1], "pmode") == 0) {
        return bno055_shell_cmd_pmode(argc, argv);
    }

    /* Dump Registers command */
    if (argc > 1 && strcmp(argv[1], "dumpreg") == 0) {
        return bno055_shell_cmd_dumpreg(argc, argv);
    }
#endif
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
