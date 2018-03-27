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
#include "hal/hal_gpio.h"
#include "bno055_priv.h"
#include "bno055/bno055.h"
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/mag.h"
#include "sensor/quat.h"
#include "sensor/euler.h"
#include "hal/hal_i2c.h"
#include "parse/parse.h"

#if MYNEWT_VAL(BNO055_CLI)

static int bno055_shell_cmd(int argc, char **argv);

static struct shell_cmd bno055_shell_cmd_struct = {
    .sc_cmd = "bno055",
    .sc_cmd_func = bno055_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(BNO055_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(BNO055_SHELL_ITF_NUM),
    .si_addr = MYNEWT_VAL(BNO055_SHELL_ITF_ADDR),
};

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
    console_printf("\tsensor_offsets\n");
    console_printf("\tdumpreg [addr]\n");

    return 0;
}

static int
bno055_shell_cmd_sensor_offsets(int argc, char **argv)
{
    int i;
    int rc;
    struct bno055_sensor_offsets bso;
    uint16_t val;
    uint16_t offsetdata[11] = {0};
    char *tok;

    rc = 0;
    if (argc > 3) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Display the chip id */
    if (argc == 2) {
        rc = bno055_get_sensor_offsets(&g_sensor_itf, &bso);
        if (rc) {
            console_printf("Read failed %d\n", rc);
            goto err;
        }
        console_printf("Offsets:\n");
        console_printf("      \tacc \t |    gyro\t |    mag \t \n"
                       "\tx  :0x%02X\t :  0x%02X\t :  0x%02X\t \n"
                       "\ty  :0x%02X\t :  0x%02X\t :  0x%02X\t \n"
                       "\tz  :0x%02X\t :  0x%02X\t :  0x%02X\t \n"
                       "\trad:0x%02X\t :        \t :  0x%02X\t \n",
                       bso.bso_acc_off_x, bso.bso_mag_off_x,
                       bso.bso_gyro_off_x, bso.bso_acc_off_y,
                       bso.bso_mag_off_y, bso.bso_gyro_off_y,
                       bso.bso_acc_off_z, bso.bso_mag_off_z,
                       bso.bso_gyro_off_z, bso.bso_acc_radius,
                       bso.bso_mag_radius);
    } else if (argc == 3) {
        tok = strtok(argv[2], ":");
        i = 0;
        do {
            val = parse_ll_bounds(tok, 0, UINT16_MAX, &rc);
            if (rc) {
                return bno055_shell_err_invalid_arg(argv[2]);
            }
            offsetdata[i] = val;
            tok = strtok(0, ":");
        } while(i++ < 11 && tok);

        bso.bso_acc_off_x  = offsetdata[0];
        bso.bso_acc_off_y  = offsetdata[1];
        bso.bso_acc_off_z  = offsetdata[2];
        bso.bso_gyro_off_x = offsetdata[3];
        bso.bso_gyro_off_y = offsetdata[4];
        bso.bso_gyro_off_z = offsetdata[5];
        bso.bso_mag_off_x  = offsetdata[6];
        bso.bso_mag_off_y  = offsetdata[7];
        bso.bso_mag_off_z  = offsetdata[8];
        bso.bso_acc_radius = offsetdata[9];
        bso.bso_mag_radius = offsetdata[10];

        rc = bno055_set_sensor_offsets(&g_sensor_itf, &bso);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
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
        rc = bno055_get_chip_id(&g_sensor_itf, &id);
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
        rc = bno055_get_rev_info(&g_sensor_itf, &ri);
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
    uint16_t val;
    int rc;
    void *databuf;
    struct sensor_quat_data *sqd;
    struct sensor_euler_data *sed;
    struct sensor_accel_data *sad;
    int8_t *temp;
    int type;
    char tmpstr[13];

    type = SENSOR_TYPE_NONE;
    if (argc > 4) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Since this is the biggest struct, malloc space for it */
    databuf = malloc(sizeof(*sqd));
    assert(databuf != NULL);


    /* Check if more than one sample requested */
    if (argc == 4) {
        val = parse_ll_bounds(argv[2], 0, UINT16_MAX, &rc);
        if (rc) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }
        samples = (uint16_t)val;

        val = parse_ll_bounds(argv[3], 0, UINT16_MAX, &rc);
        if (rc) {
            return bno055_shell_err_invalid_arg(argv[3]);
        }
        type = (int)(1 << val);
    } else {
        console_printf("Usage:\n");
        console_printf("\tr     [n_samples] [ 0-acc          | 1 -mag       | 2 -gyro    | 4 -temp   |\n"
                       "\t                    9-quat         | 26-linearacc | 27-gravity | 28-euler  ]\n\n");
    }
    while(samples--) {

        if (type == SENSOR_TYPE_ROTATION_VECTOR) {
            rc = bno055_get_quat_data(&g_sensor_itf, databuf);
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
            rc = bno055_get_vector_data(&g_sensor_itf, databuf, type);
            if (rc) {
                console_printf("Read failed: %d\n", rc);
                goto err;
            }
            sed = databuf;

            console_printf("h:%s ", sensor_ftostr(sed->sed_h, tmpstr, 13));
            console_printf("r:%s ", sensor_ftostr(sed->sed_r, tmpstr, 13));
            console_printf("p:%s\n", sensor_ftostr(sed->sed_p, tmpstr, 13));

        } else if (type == SENSOR_TYPE_TEMPERATURE) {
            rc = bno055_get_temp(&g_sensor_itf, databuf);
            if (rc) {
                console_printf("Read failed: %d\n", rc);
                goto err;
            }
            temp = databuf;

            console_printf("Temperature:%u\n", *temp);
        } else {
            rc = bno055_get_vector_data(&g_sensor_itf, databuf, type);
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
    uint8_t val;
    int rc;

    if (argc > 3) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Display the mode */
    if (argc == 2) {
        rc = bno055_get_opr_mode(&g_sensor_itf, (uint8_t *)&val);
        if (rc) {
            goto err;
        }
        console_printf("%u\n", ((unsigned int)(*(uint8_t *)&val)));
    }

    /* Update the mode */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, BNO055_OPR_MODE_NDOF, &rc);
        if (rc) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }
        /* Make sure mode is valid */
        if (val > BNO055_OPR_MODE_NDOF) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }

        rc = bno055_set_opr_mode(&g_sensor_itf, val);
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
    uint8_t val;
    int rc;

    if (argc > 3) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Display the mode */
    if (argc == 2) {
        rc = bno055_get_pwr_mode(&g_sensor_itf, (uint8_t *)&val);
        if (rc) {
            goto err;
        }
        console_printf("%u\n", (unsigned int)val);
    }

    /* Update the mode */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, BNO055_PWR_MODE_SUSPEND, &rc);
        if (rc) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }

        rc = bno055_set_pwr_mode(&g_sensor_itf, val);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
bno055_shell_units_cmd(int argc, char **argv)
{
    uint8_t val;
    int rc;

    if (argc > 3) {
        return bno055_shell_err_too_many_args(argv[1]);
    }

    /* Display the units */
    if (argc == 2) {
        rc = bno055_get_units(&g_sensor_itf, (uint8_t *)&val);
        console_printf("Acc, linear acc, gravity: %s\n"
                       "Mag field strength: Micro Tesla\n"
                       "Ang rate: %s\n"
                       "Euler ang: %s\n",
                       (uint8_t)val & BNO055_ACC_UNIT_MG ? "mg":"m/s^2",
                       (uint8_t)val & BNO055_ANGRATE_UNIT_RPS ? "Rps":"Dps",
                       (uint8_t)val & BNO055_EULER_UNIT_RAD ? "Rad":"Deg");
        console_printf("Quat: Quat units\n"
                       "Temp: %s\n"
                       "Fusion data output: %s\n",
                       (uint8_t)val & BNO055_TEMP_UNIT_DEGF ? "Deg F":"Deg C",
                       (uint8_t)val & BNO055_DO_FORMAT_ANDROID ? "Android":"Windows");
    }

    /* Update the units */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 0, UINT8_MAX, &rc);
        if (rc) {
            return bno055_shell_err_invalid_arg(argv[2]);
        }

        rc = bno055_set_units(&g_sensor_itf, val);
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
    uint8_t addr;
    uint8_t val;
    int rc;

    addr = parse_ll_bounds(argv[2], 0, UINT8_MAX, &rc);
    if (rc) {
        return bno055_shell_err_invalid_arg(argv[2]);
    }

    rc = bno055_read8(&g_sensor_itf, addr, &val);
    if (rc) {
        goto err;
    }
    console_printf("0x%02X (ADDR): 0x%02X", addr, val);

    return 0;
err:
    return rc;
}

static int
bno055_shell_cmd_reset(int argc, char **argv)
{
    int rc;
    /* Reset sensor */
    rc = bno055_write8(&g_sensor_itf, BNO055_SYS_TRIGGER_ADDR, BNO055_SYS_TRIGGER_RST_SYS);
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

    if (argc > 1 && strcmp(argv[1], "units") == 0) {
        return bno055_shell_units_cmd(argc, argv);
    }

    if (argc > 1 && strcmp(argv[1], "sensor_offsets") == 0) {
        return bno055_shell_cmd_sensor_offsets(argc, argv);
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
