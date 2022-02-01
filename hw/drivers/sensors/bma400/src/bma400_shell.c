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
#include "bma400/bma400.h"
#include "bma400_priv.h"

#if MYNEWT_VAL(BMA400_CLI) && MYNEWT_VAL(SENSOR_CLI)

#include "shell/shell.h"
#include "parse/parse.h"

#define BMA400_CLI_FIRST_REGISTER 0x00
#define BMA400_CLI_LAST_REGISTER  0x7E


static int bma400_shell_cmd(int argc, char **argv);

#if MYNEWT_VAL(SHELL_CMD_HELP)
#define HELP(a) &(bma400_##a##_help)

const struct shell_param dump_params[] = {
    { "nz",     "show registers if not zero" },
    { "acc",    "show ACC_CONFIGx" },
    { "step",   "show STEP_CONFIGxx" },
    { "orient", "show ORIENT_CONFIGx" },
    { "tap",    "show TAP_CONFIGx" },
    { "int",    "show INT_xxxx" },
    { "gen",    "show GENyINTx" },
    { "gen1",   "show GEN1INTx" },
    { "gen2",   "show GEN2INTx" },
    { "time",   "show TIMEx" },
    { "meas",   "show ACC_x_LSB/MSB" },
    { "lp",     "show AUTOLOWPOW_x" },
    { "wkup",   "show WKUP_INT_CONFIGxx" },
    { NULL, NULL },
};

static const struct shell_cmd_help bma400_dump_help = {
    .summary = "Displays bma400 registers",
    .usage = "dump "
#if MYNEWT_VAL(BMA400_CLI_DECODE)
        "[decode] "
#endif
    "[all] [nz] [acc] [step] [orient] [tap] [int] [gen[1|2]] [time] [meas] [lp] [wkup]",
    .params = dump_params,
};

#if MYNEWT_VAL(BMA400_CLI_DECODE)
static const struct shell_cmd_help bma400_decode_help = {
    .summary = "Enables or disables decoding of registers",
    .usage = "decode 1 | 0",
    .params = NULL,
};
#endif

static const struct shell_cmd_help bma400_r_help = {
    .summary = "Read sensor data",
    .usage = "r <num>",
    .params = NULL,
};

static const struct shell_cmd_help bma400_normal_help = {
    .summary = "Switches to normal mode",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help bma400_lp_help = {
    .summary = "Switches to low power mode",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help bma400_sleep_help = {
    .summary = "Switches to sleep mode",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help bma400_peek_help = {
    .summary = "Read single register value",
    .usage = "peek <addr>",
    .params = NULL,
};

static const struct shell_cmd_help bma400_poke_help = {
    .summary = "Write single register value",
    .usage = "poke <addr> <val>",
    .params = NULL,
};

static const struct shell_cmd_help bma400_fifo_help = {
    .summary = "Dumps fifo",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help bma400_test_help = {
    .summary = "Runs self-test",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help bma400_reg_cmd_help = {
    .summary = "Reads or write register",
    .usage = "<reg_name> [<reg_value>]",
    .params = NULL,
};

#if MYNEWT_VAL(bma400_USE_CHARGE_CONTROL)
static const struct shell_cmd_help bma400_listen_help = {
    .summary = "Starts or stops charging state notifications",
    .usage = "listen start | stop",
    .params = NULL,
};
#endif

#else
#define HELP(a) NULL
#endif

static struct shell_cmd bma400_shell_cmd_struct = {
    .sc_cmd = "bma400",
    .sc_cmd_func = bma400_shell_cmd
};

static struct bma400 *bma400_shell_device;

static int
bma400_shell_open_device(void)
{
    if (bma400_shell_device) {
        return 0;
    }

    bma400_shell_device = (struct bma400 *)os_dev_open(MYNEWT_VAL(BMA400_SHELL_DEV_NAME),
                                                       1000, NULL);

    if (bma400_shell_device) {
        bma400_shell_device->pdd.cache.always_read = true;
    }
    return bma400_shell_device ? 0 : SYS_ENODEV;
}

static int
bma400_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bma400_shell_err_too_few_args(char *cmd_name)
{
    console_printf("Error: too few arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bma400_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bma400_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
bma400_shell_help(void)
{
    console_printf("%s cmd [args...]\n", bma400_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tr    [<n_samples>]\n");
    console_printf("\tchipid\n");
    console_printf("\tdump [nz] [acc] [step] [orient] [tap] [int] [time] [meas] [lp] [wkup] \n");
    console_printf("\tpeek <reg>\n");
    console_printf("\tpoke <reg> <value>\n");
    console_printf("\tnormal\n");
    console_printf("\tsleep\n");
    console_printf("\tlp\n");
    console_printf("\tfifo\n");
    console_printf("\ttest\n");

    return 0;
}

static int
bma400_shell_read(uint8_t reg, uint8_t *buffer, uint8_t len)
{
    int rc = bma400_shell_open_device();
    if (rc == 0) {
        rc = bma400_read(bma400_shell_device, reg, buffer, len);
    }
    return rc;
}

static int
bma400_shell_get_register(uint8_t reg, uint8_t *val)
{
    int rc = bma400_shell_open_device();
    if (rc == 0) {
        rc = bma400_get_register(bma400_shell_device, reg, val);
    }
    return rc;
}

static int
bma400_shell_set_register(uint8_t reg, uint8_t val)
{
    int rc = bma400_shell_open_device();
    if (rc == 0) {
        rc = bma400_set_register(bma400_shell_device, reg, val);
    }
    return rc;
}

static int
bma400_shell_set_register_field(uint8_t reg, uint8_t field_mask, uint8_t field_val)
{
    int rc = bma400_shell_open_device();
    if (rc == 0) {
        rc = bma400_set_register_field(bma400_shell_device, reg, field_mask, field_val);
    }
    return rc;
}

static int
bma400_shell_self_test(bool *self_test_fail)
{
    int rc = bma400_shell_open_device();
    if (rc == 0) {
        rc = bma400_self_test(bma400_shell_device, self_test_fail);
    }
    return rc;
}

static int
bma400_shell_get_accel(float accel_data[3])
{
    int rc = bma400_shell_open_device();
    if (rc == 0) {
        rc = bma400_get_accel(bma400_shell_device, accel_data);
    }
    return rc;
}

static int
bma400_shell_get_fifo_count(uint16_t *fifo_bytes)
{
    int rc = bma400_shell_open_device();
    if (rc == 0) {
        rc = bma400_get_fifo_count(bma400_shell_device, fifo_bytes);
    }
    return rc;
}

static int
bma400_shell_read_fifo(uint16_t *fifo_count, struct sensor_accel_data *sad)
{
    int rc = bma400_shell_open_device();
    if (rc == 0) {
        rc = bma400_read_fifo(bma400_shell_device, fifo_count, sad);
    }
    return rc;
}

static int
bma400_shell_cmd_read_chipid(int argc, char **argv)
{
    int rc;
    uint8_t chipid;
    (void)argc;
    (void)argv;

    rc = bma400_shell_read(BMA400_REG_CHIPID, &chipid, 1);
    if (rc == 0) {
        console_printf("CHIP_ID:0x%02X\n", chipid);
    }

    return rc;
}

static int
bma400_shell_cmd_read(int argc, char **argv)
{
    uint16_t samples = 1;
    uint16_t val;
    int rc;
    char tmpstr[13];
    float acc[3];

    if (argc > 2) {
        return bma400_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 2) {
        val = parse_ll_bounds(argv[1], 1, UINT16_MAX, &rc);
        if (rc) {
            return bma400_shell_err_invalid_arg(argv[2]);
        }
        samples = val;
    }

    while (samples--) {

        rc = bma400_shell_get_accel(acc);
        if (rc) {
            console_printf("Read failed: %d\n", rc);
            return rc;
        }

        console_printf("x:%s ", sensor_ftostr(acc[0], tmpstr, 13));
        console_printf("y:%s ", sensor_ftostr(acc[1], tmpstr, 13));
        console_printf("z:%s\n", sensor_ftostr(acc[2], tmpstr, 13));
    }

    return 0;
}

#define BIT(n) (1 << (n))

typedef enum {
    GRP_CFG              = BIT(0),
    GRP_MEASUREMENT      = BIT(1),
    GRP_TIME             = BIT(2),
    GRP_FIFO             = BIT(3),
    GRP_STEP             = BIT(4),
    GRP_INT              = BIT(5),
    GRP_ACC              = BIT(6),
    GRP_AUTOLOWPOW       = BIT(7),
    GRP_AUTOWAKEUP       = BIT(8),
    GRP_ORIENT           = BIT(9),
    GRP_GEN1INT          = BIT(10),
    GRP_GEN2INT          = BIT(11),
    GRP_ACTIVITY         = BIT(12),
    GRP_TAP              = BIT(13),
    GRP_GLOBAL           = BIT(14),
    GRP_STATUS           = BIT(15),
    GRP_ALL              = 0xFFFF,
} bma400_reg_grp_t;

/*
 * Struct for field name description.
 * Used for argument validation and field decoding
 */
struct reg_field {
    /* Name of field */
    const char *fld_name;
    bool fld_show_bits;
    /* Mask of bit field to set or extract */
    uint8_t fld_mask;
    /* Function that will convert register value to descriptive string */
    const char *(*fld_decode_value)(const struct reg_field *field,
                                    uint8_t reg_val, char *buf);
    /* Argument that could be used by above function */
    const void *fld_arg;
};

struct bma400_reg {
    const char *reg_name;
    uint8_t reg_addr;
    bma400_reg_grp_t reg_grp;
    uint8_t seq;
    /* Array of bit fields */
    const struct reg_field *fields;
};

#if MYNEWT_VAL(BMA400_CLI_DECODE)

static bool bma400_cli_decode_fields = true;

/* Change ix value to string from table.
 * Table is NULL terminated. Access outside of range will result in
 * returning ??? string
 */
static const char *
val_decode_from_table(const char *const tab[], int ix)
{
    while (ix && tab[ix]) {
        ix--;
        tab++;
    }
    if (tab[0]) {
        return tab[0];
    } else {
        return "???";
    }
}

/* Decode bit field using table with mapped values */
static const char *
reg_decode_from_table(const struct reg_field *field,
                      uint8_t reg_val, char *buf)
{
    uint8_t val = (reg_val & field->fld_mask) >> __builtin_ctz(field->fld_mask);
    const char *const *table = field->fld_arg;

    return strcpy(buf, val_decode_from_table(table, val));
}

#define FIELD_NUM(reg, field) { .fld_name = #field, \
    .fld_show_bits = 1, .fld_mask = BMA400_##reg##_##field }
#define FIELD_TAB(reg, field, tab) { .fld_name = #field, \
    .fld_show_bits = 1, .fld_mask = BMA400_##reg##_##field, .fld_decode_value = (reg_decode_from_table), .fld_arg = (tab) }
#define FIELD_FUN(reg, field, fun, arg) { .fld_name = #field, \
    .fld_show_bits = 1, .fld_mask = BMA400_##reg##_##field, .fld_decode_value = (fun), .fld_arg = (arg) }

/* Fields for INT_STAT0 register */
static const struct reg_field INT_STAT0_fields[] = {
    FIELD_NUM(INT_STAT0, DRDY_INT_STAT),
    FIELD_NUM(INT_STAT0, FWM_INT_STAT),
    FIELD_NUM(INT_STAT0, FFULL_INT_STAT),
    FIELD_NUM(INT_STAT0, IENG_OVERRUN_STAT),
    FIELD_NUM(INT_STAT0, GEN2_INT_STAT),
    FIELD_NUM(INT_STAT0, GEN1_INT_STAT),
    FIELD_NUM(INT_STAT0, ORIENTCH_INT_STAT),
    FIELD_NUM(INT_STAT0, WKUP_INT_STAT),
    { NULL }
};

/* Fields for INT_STAT1 register */
static const struct reg_field INT_STAT1_fields[] = {
    FIELD_NUM(INT_STAT1, IENG_OVERRUN_STAT),
    FIELD_NUM(INT_STAT1, D_TAP_INT_STAT),
    FIELD_NUM(INT_STAT1, S_TAP_INT_STAT),
    FIELD_NUM(INT_STAT1, STEP_INT_STAT),
    { NULL }
};

/* Fields for INT_STAT2 register */
static const struct reg_field INT_STAT2_fields[] = {
    FIELD_NUM(INT_STAT2, IENG_OVERRUN_STAT),
    FIELD_NUM(INT_STAT2, ACTCH_Z_INT_STAT),
    FIELD_NUM(INT_STAT2, ACTCH_Y_INT_STAT),
    FIELD_NUM(INT_STAT2, ACTCH_X_INT_STAT),
    { NULL }
};

static const char *const step_stat_filed[] = {
    "still",
    "walking",
    "running",
    NULL,
};

/* Fields for STEP_STAT register */
static const struct reg_field STEP_STAT_fields[] = {
    FIELD_TAB(STEP_STAT, STEP_STAT_FIELD, step_stat_filed),
    { NULL }
};

static const char *const osr_lp_filed[] = {
    "0.4 * ODR",
    "0.2 * ODR",
    NULL,
};

static const char *const power_mode_filed[] = {
    "sleep",
    "low power",
    "normal",
    "reserved",
    NULL,
};

/* Fields for ACC_CONFIG0 register */
static const struct reg_field ACC_CONFIG0_fields[] = {
    FIELD_NUM(ACC_CONFIG0, FILT1_BW),
    FIELD_TAB(ACC_CONFIG0, OSR_LP, osr_lp_filed),
    FIELD_TAB(ACC_CONFIG0, POWER_MODE_CONF, power_mode_filed),
    { NULL }
};

static const char *const acc_range_filed[] = {
    "+/- 2g",
    "+/- 4g",
    "+/- 8g",
    "+/- 16g",
    NULL,
};

static const char *const acc_odr_filed[] = {
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "12.5 Hz",
    "25 Hz",
    "50 Hz",
    "100 Hz",
    "200 Hz",
    "400 Hz",
    "800 Hz",
    NULL,
};

/* Fields for ACC_CONFIG1 register */
static const struct reg_field ACC_CONFIG1_fields[] = {
    FIELD_TAB(ACC_CONFIG1, ACC_RANGE, acc_range_filed),
    FIELD_NUM(ACC_CONFIG1, OSR),
    FIELD_TAB(ACC_CONFIG1, ACC_ODR, acc_odr_filed),
    { NULL }
};

static const char *const data_src_reg_filed[] = {
    "acc_filt1 variable ODR filter",
    "acc_filt2 fixed 100Hz ODR filter",
    "acc_filt_lp fixed 100Hz ODR filter, 1Hz bandwitdh",
    "acc_filt1 variable ODR filter",
    NULL,
};

/* Fields for ACC_CONFIG2 register */
static const struct reg_field ACC_CONFIG2_fields[] = {
    FIELD_TAB(ACC_CONFIG2, DATA_SRC_REG, data_src_reg_filed),
    { NULL }
};

/* Fields for INT_CONFIG0 register */
static const struct reg_field INT_CONFIG0_fields[] = {
    FIELD_NUM(INT_CONFIG0, DRDY_INT_EN),
    FIELD_NUM(INT_CONFIG0, FWM_INT_EN),
    FIELD_NUM(INT_CONFIG0, FFULL_INT_EN),
    FIELD_NUM(INT_CONFIG0, GEN2_INT_EN),
    FIELD_NUM(INT_CONFIG0, GEN1_INT_EN),
    FIELD_NUM(INT_CONFIG0, ORIENTCH_INT_EN),
    { NULL }
};

/* Fields for INT_CONFIG1 register */
static const struct reg_field INT_CONFIG1_fields[] = {
    FIELD_NUM(INT_CONFIG1, LATCH_INT),
    FIELD_NUM(INT_CONFIG1, ACTCH_INT_EN),
    FIELD_NUM(INT_CONFIG1, D_TAP_INT_EN),
    FIELD_NUM(INT_CONFIG1, S_TAP_INT_EN),
    FIELD_NUM(INT_CONFIG1, STEP_INT_EN),
    { NULL }
};

/* Fields for INT1_MAP register */
static const struct reg_field INT1_MAP_fields[] = {
    FIELD_NUM(INT1_MAP, DRDY_INT1),
    FIELD_NUM(INT1_MAP, FWM_INT1),
    FIELD_NUM(INT1_MAP, FFULL_INT1),
    FIELD_NUM(INT1_MAP, IENG_OVERRUN_INT1),
    FIELD_NUM(INT1_MAP, GEN2_INT1),
    FIELD_NUM(INT1_MAP, GEN1_INT1),
    FIELD_NUM(INT1_MAP, ORIENTCH_INT1),
    FIELD_NUM(INT1_MAP, WKUP_INT1),
    { NULL }
};

/* Fields for INT2_MAP register */
static const struct reg_field INT2_MAP_fields[] = {
    FIELD_NUM(INT2_MAP, DRDY_INT2),
    FIELD_NUM(INT2_MAP, FWM_INT2),
    FIELD_NUM(INT2_MAP, FFULL_INT2),
    FIELD_NUM(INT2_MAP, IENG_OVERRUN_INT2),
    FIELD_NUM(INT2_MAP, GEN2_INT2),
    FIELD_NUM(INT2_MAP, GEN1_INT2),
    FIELD_NUM(INT2_MAP, ORIENTCH_INT2),
    FIELD_NUM(INT2_MAP, WKUP_INT2),
    { NULL }
};

/* Fields for INT12_MAP register */
static const struct reg_field INT12_MAP_fields[] = {
    FIELD_NUM(INT12_MAP, ACTCH_INT2),
    FIELD_NUM(INT12_MAP, TAP_INT2),
    FIELD_NUM(INT12_MAP, STEP_INT2),
    FIELD_NUM(INT12_MAP, ACTCH_INT1),
    FIELD_NUM(INT12_MAP, TAP_INT1),
    FIELD_NUM(INT12_MAP, STEP_INT1),
    { NULL }
};

/* Fields for INT12_IO_CTRL register */
static const struct reg_field INT12_IO_CTRL_fields[] = {
    FIELD_NUM(INT12_IO_CTRL, INT2_OD),
    FIELD_NUM(INT12_IO_CTRL, INT2_LVL),
    FIELD_NUM(INT12_IO_CTRL, INT1_OD),
    FIELD_NUM(INT12_IO_CTRL, INT1_LVL),
    { NULL }
};

/* Fields for FIFO_CONFIG0 register */
static const struct reg_field FIFO_CONFIG0_fields[] = {
    FIELD_NUM(FIFO_CONFIG0, FIFO_Z_EN),
    FIELD_NUM(FIFO_CONFIG0, FIFO_Y_EN),
    FIELD_NUM(FIFO_CONFIG0, FIFO_X_EN),
    FIELD_NUM(FIFO_CONFIG0, FIFO_8BIT_EN),
    FIELD_NUM(FIFO_CONFIG0, FIFO_DATA_SRC),
    FIELD_NUM(FIFO_CONFIG0, FIFO_TIME_EN),
    FIELD_NUM(FIFO_CONFIG0, FIFO_STOP_ON_FULL),
    FIELD_NUM(FIFO_CONFIG0, AUTO_FLUSH),
    { NULL }
};

/* Fields for FIFO_PWR_CONFIG register */
static const struct reg_field FIFO_PWR_CONFIG_fields[] = {
    FIELD_NUM(FIFO_PWR_CONFIG, FIFO_READ_DISABLE),
    { NULL }
};

static const char *const auto_lp_timeout_filed[] = {
    "0 Low-power timeout disabled",
    "1 Low-power timeout active, device shall switch into low power mode as soon timeout counter is expired",
    "2 Low-power timeout active, as 0x01, but timeout counter resets if gen2_int is asserted",
    "3 Low-power timeout active, device shall switch into low power mode as soon timeout counter is expired",
    NULL,
};

/* Fields for AUTOLOWPOW_1 register */
static const struct reg_field AUTOLOWPOW_1_fields[] = {
    FIELD_NUM(AUTOLOWPOW_1, AUTO_LP_TIMEOUT_THRES),
    FIELD_TAB(AUTOLOWPOW_1, AUTO_LP_TIMEOUT, auto_lp_timeout_filed),
    FIELD_NUM(AUTOLOWPOW_1, GEN1_INT),
    FIELD_NUM(AUTOLOWPOW_1, DRDY_LOWPOW_TRIG),
    { NULL }
};

/* Fields for AUTOWAKEUP_1 register */
static const struct reg_field AUTOWAKEUP_1_fields[] = {
    FIELD_NUM(AUTOWAKEUP_1, WAKEUP_TIMEOUT_THRES),
    FIELD_NUM(AUTOWAKEUP_1, WKUP_TIMEOUT),
    FIELD_NUM(AUTOWAKEUP_1, WKUP_INT),
    { NULL }
};

static const char *const wkup_refu_field[] = {
    "manual update (reference registers are updated by external MCU)",
    "one time automated update before going into low power mode",
    "every time after data conversion",
    NULL,
};

/* Fields for WKUP_INT_CONFIG0 register */
static const struct reg_field WKUP_INT_CONFIG0_fields[] = {
    FIELD_NUM(WKUP_INT_CONFIG0, WKUP_Z_EN),
    FIELD_NUM(WKUP_INT_CONFIG0, WKUP_Y_EN),
    FIELD_NUM(WKUP_INT_CONFIG0, WKUP_X_EN),
    FIELD_NUM(WKUP_INT_CONFIG0, NUM_OF_SAMPLES),
    FIELD_TAB(WKUP_INT_CONFIG0, WKUP_REFU, wkup_refu_field),
    { NULL }
};

static const char *const orient_data_src_field[] = {
    "acc_filt2",
    "acc_filt_lp",
    NULL,
};

static const char *const orient_refu_field[] = {
    "manual update (reference registers are updated by serial interface command)",
    "one time automated update using acc_filt2 data",
    "one time automated update using acc_filt_lp data",
    NULL,
};

/* Fields for ORIENTCH_CONFIG0 register */
static const struct reg_field ORIENTCH_CONFIG0_fields[] = {
    FIELD_NUM(ORIENTCH_CONFIG0, ORIENT_Z_EN),
    FIELD_NUM(ORIENTCH_CONFIG0, ORIENT_Y_EN),
    FIELD_NUM(ORIENTCH_CONFIG0, ORIENT_X_EN),
    FIELD_TAB(ORIENTCH_CONFIG0, ORIENT_DATA_SRC, orient_data_src_field),
    FIELD_TAB(ORIENTCH_CONFIG0, ORIENT_REFU, orient_refu_field),
    { NULL }
};


static const char *const gen1_data_src_field[] = {
    "acc_filt1",
    "acc_filt2",
    NULL,
};

static const char *const gen1_act_refu_field[] = {
    "manual update (reference registers are updated by a serial interface command)",
    "one time automated update by the selected data source",
    "every time automated update by the selected data source",
    "every time automated update by acc_filt_lp",
    NULL,
};

static const char *const gen1_act_hyst_field[] = {
    "no hysteresis",
    "24mg hysteresis",
    "48mg hysteresis",
    "96mg hysteresis",
    NULL,
};

/* Fields for GEN1INT_CONFIG0 register */
static const struct reg_field GEN1INT_CONFIG0_fields[] = {
    FIELD_NUM(GEN1INT_CONFIG0, GEN1_ACT_Z_EN),
    FIELD_NUM(GEN1INT_CONFIG0, GEN1_ACT_Y_EN),
    FIELD_NUM(GEN1INT_CONFIG0, GEN1_ACT_X_EN),
    FIELD_TAB(GEN1INT_CONFIG0, GEN1_DATA_SRC, gen1_data_src_field),
    FIELD_TAB(GEN1INT_CONFIG0, GEN1_ACT_REFU, gen1_act_refu_field),
    FIELD_TAB(GEN1INT_CONFIG0, GEN1_ACT_HYST, gen1_act_hyst_field),
    { NULL }
};

static const char *const gen1_criterion_sel_field[] = {
    "acceleration below threshold: inactivity detection",
    "acceleration above threshold: activity detection",
    NULL,
};

static const char *const gen1_comb_sel_field[] = {
    "OR combination of x/y/z axis evaluation results",
    "AND combination of x/y/z axis evaluation results",
    NULL,
};

/* Fields for GEN1INT_CONFIG1 register */
static const struct reg_field GEN1INT_CONFIG1_fields[] = {
    FIELD_TAB(GEN1INT_CONFIG1, GEN1_CRITERION_SEL, gen1_criterion_sel_field),
    FIELD_TAB(GEN1INT_CONFIG1, GEN1_COMB_SEL, gen1_comb_sel_field),
    { NULL }
};


/* Fields for GEN2INT_CONFIG0 register */
static const struct reg_field GEN2INT_CONFIG0_fields[] = {
    FIELD_NUM(GEN2INT_CONFIG0, GEN2_ACT_Z_EN),
    FIELD_NUM(GEN2INT_CONFIG0, GEN2_ACT_Y_EN),
    FIELD_NUM(GEN2INT_CONFIG0, GEN2_ACT_X_EN),
    FIELD_TAB(GEN2INT_CONFIG0, GEN2_DATA_SRC, gen1_data_src_field),
    FIELD_TAB(GEN2INT_CONFIG0, GEN2_ACT_REFU, gen1_act_refu_field),
    FIELD_TAB(GEN2INT_CONFIG0, GEN2_ACT_HYST, gen1_act_hyst_field),
    { NULL }
};

/* Fields for GEN2INT_CONFIG1 register */
static const struct reg_field GEN2INT_CONFIG1_fields[] = {
    FIELD_TAB(GEN2INT_CONFIG1, GEN2_CRITERION_SEL, gen1_criterion_sel_field),
    FIELD_TAB(GEN2INT_CONFIG1, GEN2_COMB_SEL, gen1_comb_sel_field),
    { NULL }
};

#define actch_data_src_field gen1_data_src_field

static const char *const actch_npts_field[] = {
    "32 points",
    "64 points",
    "128 points",
    "256 points",
    "512 points",
    NULL,
};

/* Fields for ACTCH_CONFIG1 register */
static const struct reg_field ACTCH_CONFIG1_fields[] = {
    FIELD_NUM(ACTCH_CONFIG1, ACTCH_Z_EN),
    FIELD_NUM(ACTCH_CONFIG1, ACTCH_Y_EN),
    FIELD_NUM(ACTCH_CONFIG1, ACTCH_X_EN),
    FIELD_TAB(ACTCH_CONFIG1, ACTCH_DATA_SRC, actch_data_src_field),
    FIELD_TAB(ACTCH_CONFIG1, ACTCH_NPTS, actch_npts_field),
    { NULL }
};

static const char *const sel_axis_field[] = {
    "use Z axis data",
    "use Y axis data",
    "use X axis data",
    NULL,
};

/* Fields for TAP_CONFIG register */
static const struct reg_field TAP_CONFIG_fields[] = {
    FIELD_TAB(TAP_CONFIG, SEL_AXIS, sel_axis_field),
    FIELD_NUM(TAP_CONFIG, TAP_SENSITIVITY),
    { NULL }
};

static const char *const quiet_dt_field[] = {
    "4 data samples minimum time between double taps",
    "8 data samples minimum time between double taps",
    "12 data samples minimum time between double taps",
    "16 data samples minimum time between double taps",
    NULL,
};

static const char *const quiet_field[] = {
    "60 data samples quiet tie between single or doube taps",
    "80 data samples quiet tie between single or doube taps",
    "100 data samples quiet tie between single or doube taps",
    "120 data samples quiet tie between single or doube taps",
    NULL,
};

static const char *const tics_th_field[] = {
    "6 data samples for high-low tap signal change time",
    "9 data samples for high-low tap signal change time",
    "12 data samples for high-low tap signal change time",
    "18 data samples for high-low tap signal change time",
    NULL,
};

/* Fields for TAP_CONFIG1 register */
static const struct reg_field TAP_CONFIG1_fields[] = {
    FIELD_TAB(TAP_CONFIG1, QUIET_DT, quiet_dt_field),
    FIELD_TAB(TAP_CONFIG1, QUIET, quiet_field),
    FIELD_TAB(TAP_CONFIG1, TICS_TH, tics_th_field),
    { NULL }
};

static const char *
field_bit_string(const struct reg_field *field, uint8_t val, char *buf)
{
    int lsb = __builtin_ctz(field->fld_mask);
    int msb = lsb + __builtin_popcount(field->fld_mask) - 1;
    int i;

    buf[8] = '\0';
    for (i = 0; i < 8; ++i) {
        if (i >= lsb && i <= msb) {
            buf[7 - i] = (char)('0' + (val & 1));
        } else {
            buf[7 - i] = '.';
        }
        val >>= 1;
    }

    return buf;
}

static int
field_int_value(const struct reg_field *field, uint32_t val)
{
    uint8_t fld_pos = __builtin_ctz(field->fld_mask);
    return (int)((val & field->fld_mask) >> fld_pos);
}

#define REG(name, grp) { .reg_name = #name, .reg_addr = BMA400_REG_ ## name, .reg_grp = (grp), .seq = 0, .fields = name##_fields }

static int
bma400_shell_cmd_decode(int argc, char **argv)
{
    long long val;
    int status;

    if (argc == 2) {
        val = parse_ll_bounds(argv[1], 0, 1, &status);
        if (status == 0) {
            bma400_cli_decode_fields = val != 0;
        }
    }
    console_printf("decode %d\n", (int)bma400_cli_decode_fields);

    return 0;
}

#else
#define bma400_cli_decode_fields false

#define REG(name, grp) { .reg_name = #name, .reg_addr = BMA400_REG_ ## name, .reg_grp = (grp), .seq = 0, .fields = NULL }
#endif

#define REGS(name, grp, seq) { .reg_name = #name, .reg_addr = BMA400_REG_ ## name, .reg_grp = (grp), seq, .fields = NULL }
#define REGNF(name, grp) { .reg_name = #name, .reg_addr = BMA400_REG_ ## name, .reg_grp = (grp), .seq = 0, .fields = NULL }

const struct bma400_reg bma400_regs[] = {
    /* Dump all the register values for debug purposes */
    REGNF(CHIPID, GRP_GLOBAL),
    REGNF(ERR_REG, GRP_GLOBAL),
    REGNF(STATUS, GRP_GLOBAL),
    REGS(ACC_X_LSB, GRP_MEASUREMENT, 0),
    REGS(ACC_X_MSB, GRP_MEASUREMENT, 1),
    REGS(ACC_Y_LSB, GRP_MEASUREMENT, 0),
    REGS(ACC_Y_MSB, GRP_MEASUREMENT, 1),
    REGS(ACC_Z_LSB, GRP_MEASUREMENT, 0),
    REGS(ACC_Z_MSB, GRP_MEASUREMENT, 1),
    REGS(SENSOR_TIME0, GRP_TIME, 0),
    REGS(SENSOR_TIME1, GRP_TIME, 1),
    REGS(SENSOR_TIME2, GRP_TIME, 2),
    REGNF(EVENT, GRP_STATUS),
    REG(INT_STAT0, GRP_INT | GRP_GEN1INT | GRP_GEN2INT | GRP_ORIENT | GRP_AUTOWAKEUP),
    REG(INT_STAT1, GRP_INT | GRP_TAP | GRP_STEP),
    REG(INT_STAT2, GRP_INT | GRP_ACTIVITY),
    REGNF(TEMP_DATA, GRP_MEASUREMENT),
    REGS(FIFO_LENGTH0, GRP_FIFO, 0),
    REGS(FIFO_LENGTH1, GRP_FIFO, 1),
    REGS(STEP_CNT_0, GRP_STEP, 0),
    REGS(STEP_CNT_1, GRP_STEP, 1),
    REGS(STEP_CNT_2, GRP_STEP, 2),
    REG(STEP_STAT, GRP_STEP),
    REG(ACC_CONFIG0, GRP_ACC | GRP_CFG),
    REG(ACC_CONFIG1, GRP_ACC | GRP_CFG),
    REG(ACC_CONFIG2, GRP_ACC | GRP_CFG),
    REG(INT_CONFIG0, GRP_INT | GRP_CFG | GRP_GEN1INT | GRP_GEN2INT | GRP_ORIENT),
    REG(INT_CONFIG1, GRP_INT | GRP_CFG | GRP_TAP | GRP_STEP),
    REG(INT1_MAP, GRP_INT | GRP_CFG | GRP_GEN1INT | GRP_GEN2INT | GRP_ORIENT | GRP_AUTOWAKEUP),
    REG(INT2_MAP, GRP_INT | GRP_CFG | GRP_GEN1INT | GRP_GEN2INT | GRP_ORIENT | GRP_AUTOWAKEUP),
    REG(INT12_MAP, GRP_INT | GRP_CFG | GRP_TAP | GRP_ACTIVITY | GRP_STEP),
    REG(INT12_IO_CTRL, GRP_INT | GRP_CFG),
    REG(FIFO_CONFIG0, GRP_FIFO | GRP_CFG),
    REGNF(FIFO_CONFIG1, GRP_FIFO | GRP_CFG),
    REGNF(FIFO_CONFIG2, GRP_FIFO | GRP_CFG),
    REG(FIFO_PWR_CONFIG, GRP_FIFO | GRP_CFG),
    REGNF(AUTOLOWPOW_0, GRP_AUTOLOWPOW | GRP_CFG),
    REG(AUTOLOWPOW_1, GRP_AUTOLOWPOW | GRP_CFG),
    REGNF(AUTOWAKEUP_0, GRP_AUTOWAKEUP | GRP_CFG),
    REG(AUTOWAKEUP_1, GRP_AUTOWAKEUP | GRP_CFG),
    REG(WKUP_INT_CONFIG0, GRP_AUTOWAKEUP | GRP_CFG),
    REGNF(WKUP_INT_CONFIG1, GRP_AUTOWAKEUP | GRP_CFG),
    REGNF(WKUP_INT_CONFIG2, GRP_AUTOWAKEUP | GRP_CFG),
    REGNF(WKUP_INT_CONFIG3, GRP_AUTOWAKEUP | GRP_CFG),
    REGNF(WKUP_INT_CONFIG4, GRP_AUTOWAKEUP | GRP_CFG),
    REG(ORIENTCH_CONFIG0, GRP_ORIENT | GRP_CFG),
    REGNF(ORIENTCH_CONFIG1, GRP_ORIENT | GRP_CFG),
    REGNF(ORIENTCH_CONFIG3, GRP_ORIENT | GRP_CFG),
    REGNF(ORIENTCH_CONFIG4, GRP_ORIENT | GRP_CFG),
    REGNF(ORIENTCH_CONFIG5, GRP_ORIENT | GRP_CFG),
    REGNF(ORIENTCH_CONFIG6, GRP_ORIENT | GRP_CFG),
    REGNF(ORIENTCH_CONFIG7, GRP_ORIENT | GRP_CFG),
    REGNF(ORIENTCH_CONFIG8, GRP_ORIENT | GRP_CFG),
    REGNF(ORIENTCH_CONFIG9, GRP_ORIENT | GRP_CFG),
    REG(GEN1INT_CONFIG0, GRP_GEN1INT | GRP_CFG),
    REG(GEN1INT_CONFIG1, GRP_GEN1INT | GRP_CFG),
    REGNF(GEN1INT_CONFIG2, GRP_GEN1INT | GRP_CFG),
    REGNF(GEN1INT_CONFIG3, GRP_GEN1INT | GRP_CFG),
    REGNF(GEN1INT_CONFIG31, GRP_GEN1INT | GRP_CFG),
    REGNF(GEN1INT_CONFIG4, GRP_GEN1INT | GRP_CFG),
    REGNF(GEN1INT_CONFIG5, GRP_GEN1INT | GRP_CFG),
    REGNF(GEN1INT_CONFIG6, GRP_GEN1INT | GRP_CFG),
    REGNF(GEN1INT_CONFIG7, GRP_GEN1INT | GRP_CFG),
    REGNF(GEN1INT_CONFIG8, GRP_GEN1INT | GRP_CFG),
    REGNF(GEN1INT_CONFIG9, GRP_GEN1INT | GRP_CFG),
    REG(GEN2INT_CONFIG0, GRP_GEN2INT | GRP_CFG),
    REG(GEN2INT_CONFIG1, GRP_GEN2INT | GRP_CFG),
    REGNF(GEN2INT_CONFIG2, GRP_GEN2INT | GRP_CFG),
    REGNF(GEN2INT_CONFIG3, GRP_GEN2INT | GRP_CFG),
    REGNF(GEN2INT_CONFIG31, GRP_GEN2INT | GRP_CFG),
    REGNF(GEN2INT_CONFIG4, GRP_GEN2INT | GRP_CFG),
    REGNF(GEN2INT_CONFIG5, GRP_GEN2INT | GRP_CFG),
    REGNF(GEN2INT_CONFIG6, GRP_GEN2INT | GRP_CFG),
    REGNF(GEN2INT_CONFIG7, GRP_GEN2INT | GRP_CFG),
    REGNF(GEN2INT_CONFIG8, GRP_GEN2INT | GRP_CFG),
    REGNF(GEN2INT_CONFIG9, GRP_GEN2INT | GRP_CFG),
    REGNF(ACTCH_CONFIG0, GRP_ACTIVITY | GRP_CFG),
    REG(ACTCH_CONFIG1, GRP_ACTIVITY | GRP_CFG),
    REG(TAP_CONFIG, GRP_TAP | GRP_CFG),
    REG(TAP_CONFIG1, GRP_TAP | GRP_CFG),
    REGNF(IF_CONF, GRP_GLOBAL),
    REGNF(SELF_TEST, GRP_GLOBAL),
    REGNF(STEP_COUNTER_CONFIG0, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG1, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG2, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG3, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG4, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG5, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG6, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG7, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG8, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG9, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG10, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG11, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG12, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG13, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG14, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG15, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG16, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG17, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG18, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG19, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG20, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG21, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG22, GRP_STEP | GRP_CFG),
    REGNF(STEP_COUNTER_CONFIG23, GRP_STEP | GRP_CFG),
};
#define NUM_BMA400_REGS ARRAY_SIZE(bma400_regs)

static void
bma400_shell_dump_register(const struct bma400_reg *reg, uint8_t val, bool non_zero_only, bool decode)
{
#if MYNEWT_VAL(BMA400_CLI_DECODE)
    const struct reg_field *field;
    char binary[9];
    char buf[100];
    uint8_t bit_field_val;
#endif
    if (MYNEWT_VAL(BMA400_CLI_DECODE) == 0 || !non_zero_only || val != 0) {
        console_printf("%-22s = 0x%02x \n", reg->reg_name, (int)val);
    }
#if MYNEWT_VAL(BMA400_CLI_DECODE)
    if (decode && reg->fields) {
        for (field = reg->fields; field->fld_name; ++field) {
            if (field->fld_decode_value) {
                field->fld_decode_value(field, val, buf);
                if (field->fld_show_bits) {
                    console_printf("%22s = %s %s\n", field->fld_name,
                                   field_bit_string(field, val, binary), buf);
                } else {
                    console_printf("%22s = %s\n", field->fld_name, buf);
                }
            } else {
                bit_field_val = field_int_value(field, val);
                if (!non_zero_only || bit_field_val != 0) {
                    console_printf("%22s = %s %d\n", field->fld_name,
                                   field_bit_string(field, val, binary),
                                   bit_field_val);
                }
            }
        }
    }
#endif
}

static int
bma400_shell_cmd_dump(int argc, char **argv)
{
    int i;
    int rc = 0;
    uint8_t val;
    bma400_reg_grp_t sel = 0;
    bool non_zero_only = false;
    bool decode = bma400_cli_decode_fields;

    for (i = 1; i < argc; ++i) {
        if (0 == strcmp(argv[i], "all")) {
            sel = 0xFFFF;
            non_zero_only = false;
        } else if (0 == strcmp(argv[i], "nz")) {
            non_zero_only = true;
        } else if (0 == strcmp(argv[i], "acc")) {
            sel |= GRP_ACC;
        } else if (0 == strcmp(argv[i], "step")) {
            sel |= GRP_STEP;
        } else if (0 == strcmp(argv[i], "int")) {
            sel |= GRP_INT;
        } else if (0 == strcmp(argv[i], "orient")) {
            sel |= GRP_ORIENT;
        } else if (0 == strcmp(argv[i], "lp")) {
            sel |= GRP_AUTOLOWPOW;
        } else if (0 == strcmp(argv[i], "wkup")) {
            sel |= GRP_AUTOWAKEUP;
        } else if (0 == strcmp(argv[i], "tap")) {
            sel |= GRP_TAP;
        } else if (0 == strcmp(argv[i], "gen1")) {
            sel |= GRP_GEN1INT;
        } else if (0 == strcmp(argv[i], "gen2")) {
            sel |= GRP_GEN2INT;
        } else if (0 == strcmp(argv[i], "gen")) {
            sel |= GRP_GEN1INT | GRP_GEN2INT;
        } else if (0 == strcmp(argv[i], "time")) {
            sel |= GRP_TIME;
        } else if (0 == strcmp(argv[i], "meas")) {
            sel |= GRP_MEASUREMENT;
        } else if (0 == strcmp(argv[i], "fifo")) {
            sel |= GRP_FIFO;
#if MYNEWT_VAL(BMA400_CLI_DECODE)
        } else if (0 == strcmp(argv[i], "decode")) {
            decode = true;
#endif
        }
    }
    if (sel == 0) {
        sel = GRP_ALL;
    }
    for (i = 0; rc == 0 && i < ARRAY_SIZE(bma400_regs); ++i) {
        if (bma400_regs[i].reg_grp & sel) {
            rc = bma400_shell_get_register(bma400_regs[i].reg_addr, &val);
            if (rc) {
                console_printf("Error reading register 0x%X (%s), rc = %d\n", bma400_regs[i].reg_addr,
                               bma400_regs[i].reg_name, rc);
            } else {
                bma400_shell_dump_register(&bma400_regs[i], val, non_zero_only, decode);
            }
        }
    }

    return 0;
}

static int
bma400_shell_cmd_peek(int argc, char **argv)
{
    int rc;
    uint8_t value;
    uint8_t reg;

    if (argc > 3) {
        return bma400_shell_err_too_many_args(argv[1]);
    } else if (argc < 3) {
        return bma400_shell_err_too_few_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], BMA400_CLI_FIRST_REGISTER, BMA400_CLI_LAST_REGISTER, &rc);
    if (rc != 0) {
        return bma400_shell_err_invalid_arg(argv[2]);
    }

    rc = bma400_shell_get_register(reg, &value);
    if (rc) {
        console_printf("peek failed %d\n", rc);
    }else{
        console_printf("reg 0x%02X(%d) = 0x%02X\n", reg, reg, value);
    }

    return 0;
}

static int
bma400_shell_cmd_poke(int argc, char **argv)
{
    int rc;
    uint8_t reg;
    uint8_t value;

    if (argc > 4) {
        return bma400_shell_err_too_many_args(argv[1]);
    } else if (argc < 4) {
        return bma400_shell_err_too_few_args(argv[1]);
    }

    reg = parse_ll_bounds(argv[2], BMA400_CLI_FIRST_REGISTER, BMA400_CLI_LAST_REGISTER, &rc);
    if (rc != 0) {
        return bma400_shell_err_invalid_arg(argv[2]);
   }

    value = parse_ll_bounds(argv[3], 0, 255, &rc);
    if (rc != 0) {
        return bma400_shell_err_invalid_arg(argv[3]);
    }

    rc = bma400_shell_set_register(reg, value);
    if (rc) {
        console_printf("poke failed %d\n", rc);
    }else{
        console_printf("wrote: 0x%02X(%d) to 0x%02X\n", value, value, reg);
    }

    return 0;
}

static int
bma400_shell_cmd_fifo(int argc, char **argv)
{
    int rc;
    uint16_t fifo_count;
    struct sensor_accel_data sad;
    char tmpstr[13];
    int n = 0;
    (void)argc;
    (void)argv;

    rc = bma400_shell_get_fifo_count(&fifo_count);
    if (rc == 0) {
        if (fifo_count == 0) {
            console_printf("FIFO empty\n");
            return 0;
        }
    }
    while (fifo_count && rc == 0) {
        rc = bma400_shell_read_fifo(&fifo_count, &sad);
        if (rc == 0) {
            ++n;
            console_printf("%d ", n);
            if (sad.sad_x_is_valid) {
                console_printf("x:%s ", sensor_ftostr(sad.sad_x, tmpstr, 13));
            }
            if (sad.sad_y_is_valid) {
                console_printf("y:%s ", sensor_ftostr(sad.sad_y, tmpstr, 13));
            }
            if (sad.sad_y_is_valid) {
                console_printf("z:%s ", sensor_ftostr(sad.sad_z, tmpstr, 13));
            }
            console_printf("\n");
        }
    }

    return 0;
}

static void
bma400_shell_set_power_mode(bma400_power_mode_t mode)
{
    int rc;

    rc = bma400_shell_set_register_field(BMA400_REG_ACC_CONFIG0, BMA400_ACC_CONFIG0_POWER_MODE_CONF, mode);
    if (rc) {
        console_printf("BMA400 communication failed %d\n", rc);
    }
}
static int
bma400_shell_cmd_sleep(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    bma400_shell_set_power_mode(BMA400_POWER_MODE_SLEEP);

    return 0;
}

static int
bma400_shell_cmd_lp(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    bma400_shell_set_power_mode(BMA400_POWER_MODE_LOW);

    return 0;
}

static int
bma400_shell_cmd_normal(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    bma400_shell_set_power_mode(BMA400_POWER_MODE_NORMAL);

    return 0;
}

static int
bma400_shell_cmd_test(int argc, char **argv)
{
    int rc;
    bool result = 0;
    (void)argc;
    (void)argv;

    rc = bma400_shell_self_test(&result);
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
bma400_reg_cmd(int argc, char **argv)
{
    const struct bma400_reg *reg = NULL;
    uint8_t val;
    uint8_t old_val;
    int status;
    int i;
    int add;
    int remove;
    int n;
    bool dump = false;
    bool decode = bma400_cli_decode_fields;

    for (i = 0; i < NUM_BMA400_REGS; ++i) {
        if (strcasecmp(bma400_regs[i].reg_name, argv[0]) == 0) {
            reg = bma400_regs + i;
        }
    }
    if (!reg) {
        return 0;
    }

    if (argc == 1) {
        dump = true;
#if MYNEWT_VAL(BMA400_CLI_DECODE)
    } else if (strcmp(argv[1], "decode") == 0) {
        dump = true;
        decode = true;
#endif
    } else {
        add = argv[1][0] == '+';
        remove = argv[1][0] == '-';
        n = add | remove;
        val = (uint32_t)parse_ull_bounds(&argv[1][n], 0, 0xFFFFFFFF, &status);
        if (status) {
            console_printf("Invalid register value %s\n", argv[1]);
            return 0;
        }

    }
    if (dump) {
        bma400_shell_get_register(reg->reg_addr, &val);
        bma400_shell_dump_register(reg, val, false, decode);
    } else {
        if (add | remove) {
            bma400_shell_get_register(reg->reg_addr, &old_val);
            if (add) {
                val |= old_val;
            } else {
                val = old_val & ~(val);
            }
        }
        bma400_shell_set_register(reg->reg_addr, val);
    }

    return 0;
}

#define REG_NAME(short_name) #short_name

static const struct shell_cmd bma400_cmds[] = {
#if MYNEWT_VAL(BMA400_CLI_DECODE)
    SHELL_CMD("decode", bma400_shell_cmd_decode, HELP(decode)),
#endif
    SHELL_CMD("r", bma400_shell_cmd_read, HELP(r)),
    SHELL_CMD("dump", bma400_shell_cmd_dump, HELP(dump)),
    SHELL_CMD("chipid", bma400_shell_cmd_read_chipid, NULL),
    SHELL_CMD("peek", bma400_shell_cmd_peek, HELP(peek)),
    SHELL_CMD("poke", bma400_shell_cmd_poke, HELP(poke)),
    SHELL_CMD("fifo", bma400_shell_cmd_fifo, HELP(fifo)),
    SHELL_CMD("sleep", bma400_shell_cmd_sleep, HELP(sleep)),
    SHELL_CMD("lp", bma400_shell_cmd_lp, HELP(lp)),
    SHELL_CMD("normal", bma400_shell_cmd_normal, HELP(normal)),
    SHELL_CMD("test", bma400_shell_cmd_test, HELP(test)),
    SHELL_CMD(REG_NAME(ACC_CONFIG0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ACC_CONFIG1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ACC_CONFIG2), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(INT_CONFIG0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(INT_CONFIG1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(INT1_MAP), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(INT2_MAP), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(INT12_MAP), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(INT12_IO_CTRL), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(FIFO_CONFIG0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(FIFO_CONFIG1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(FIFO_CONFIG2), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(FIFO_PWR_CONFIG), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(AUTOLOWPOW_0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(AUTOLOWPOW_1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(AUTOWAKEUP_0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(AUTOWAKEUP_1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(WKUP_INT_CONFIG0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(WKUP_INT_CONFIG1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(WKUP_INT_CONFIG2), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(WKUP_INT_CONFIG3), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(WKUP_INT_CONFIG4), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ORIENTCH_CONFIG0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ORIENTCH_CONFIG1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ORIENTCH_CONFIG3), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ORIENTCH_CONFIG4), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ORIENTCH_CONFIG5), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ORIENTCH_CONFIG6), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ORIENTCH_CONFIG7), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ORIENTCH_CONFIG8), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ORIENTCH_CONFIG9), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG2), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG3), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG31), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG4), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG5), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG6), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG7), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG8), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN1INT_CONFIG9), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG2), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG3), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG31), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG4), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG5), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG6), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG7), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG8), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(GEN2INT_CONFIG9), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ACTCH_CONFIG0), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(ACTCH_CONFIG1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(TAP_CONFIG), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(REG_NAME(TAP_CONFIG1), bma400_reg_cmd, HELP(reg_cmd)),
    SHELL_CMD(NULL, NULL, NULL)
};

static int
bma400_shell_cmd(int argc, char **argv)
{
    const struct shell_cmd *cmd;

    if (argc == 1) {
        return bma400_shell_help();
    }

    for (cmd = bma400_cmds; cmd->sc_cmd; ++cmd) {
        if (strcmp(argv[1], cmd->sc_cmd) == 0) {
            return cmd->sc_cmd_func(argc - 1, &argv[1]);
        }
    }

    return bma400_shell_err_unknown_arg(argv[1]);
}

int
bma400_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&bma400_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = shell_register("bma400", bma400_cmds);

    return rc;
}

#endif
