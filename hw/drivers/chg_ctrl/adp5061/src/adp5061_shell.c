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
#include <os/mynewt.h>

#if MYNEWT_VAL(ADP5061_CLI)

#include <console/console.h>
#include <shell/shell.h>
#include <adp5061/adp5061.h>
#include <parse/parse.h>
#include <stdio.h>
#include "adp5061_priv.h"

static struct adp5061_dev *adp5061_dev;

/*
 * Struct for field name description.
 * Used for argument validation and field decoding
 */
struct reg_field
{
    /* Name of field */
    const char *fld_name;
    /* Position of field in register (LSB position) */
    int fld_pos;
    /* Mask of bit field to set or extract */
    uint32_t fld_mask;
    /* Function that will convert register value to descriptive string */
    const char *(*fld_decode_value)(const struct reg_field *field,
            uint32_t reg_val, char *buf);
    /* Argument passed to function above */
    const void *fld_arg;
};

/**
* ADP5061 Register structure
*/
struct adp5061_reg {
    /* Address of the register */
    uint8_t addr;
    /* Name of the register */
    const char *name;
    /* Array of bit fields */
    const struct reg_field *fields;
};

#if MYNEWT_VAL(ADP5061_CLI_DECODE)

/* Change ix value to string from table.
 * Table is NULL terminated. Access outside of range will result in
 * returning ??? string
 */
static const char *val_decode_from_table(const char *const tab[], int ix)
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
static const char *reg_decode_from_table(const struct reg_field *field,
                                         uint32_t reg_val, char *buf)
{
    uint32_t val = (reg_val & field->fld_mask) >> field->fld_pos;
    const char * const *table = field->fld_arg;

    return val_decode_from_table(table, val);
}

static const char *const ilim_desc[] = {
        "100 mA",
        "150 mA",
        "200 mA",
        "250 mA",
        "300 mA",
        "400 mA",
        "500 mA",
        "600 mA",
        "700 mA",
        "800 mA",
        "900 mA",
        "1000 mA",
        "1200 mA",
        "1500 mA",
        "1800 mA",
        "2100 mA",
};

/* Fields for VINx register */
static const struct reg_field adp_reg_vinx_fields[] = {
        { "ILIM",   0,  0xF, reg_decode_from_table, ilim_desc },
        { NULL }
};

static const char *
vtrm_decode(const struct reg_field *field, uint32_t reg_val, char *buf)
{
    int v;
    reg_val = (reg_val & field->fld_mask) >> field->fld_pos;

    if (reg_val < 15) {
        strcpy(buf, "???");
        return buf;
    }
    if (reg_val >= 51) {
        reg_val = 51;
    }
    v = 380 + (reg_val - 15) * 2;
    sprintf(buf, "%d.%02d V", v / 100, v % 100);

    return buf;
}

static const char *const chg_vlim_desc[] = {
        "3.2 V",
        "3.4 V",
        "3.7 V",
        "3.8 V",
};

/* Fields for Termination settings register */
static const struct reg_field adp_reg_term_fields[] = {
        { "VTRM",       2,  0xFC, vtrm_decode, NULL },
        { "CHG_VLIM",   0,  0x03, reg_decode_from_table, chg_vlim_desc },
        { NULL }
};

static const char *
ichg_decode(const struct reg_field *field, uint32_t reg_val, char *buf)
{
    int a;
    reg_val = (reg_val & field->fld_mask) >> field->fld_pos;

    if (reg_val > 22) {
        reg_val = 24;
    }
    a = 50 * (reg_val + 1);
    sprintf(buf, "%02d mA", a);

    return buf;
}

static const char *
write_only_field(const struct reg_field *field, uint32_t reg_val, char *buf)
{
    (void)field;
    (void)reg_val;

    strcpy(buf, "??? write only");
    return buf;
}

static const char *const itrk_dead_desc[] = {
        "5 mA",
        "10 mA",
        "20 mA",
        "80 mA",
};

/* Fields for Charging current register */
static const struct reg_field adp_reg_chrg_curr_fields[] = {
        { "ICHG",       2,  0x7C, ichg_decode, NULL },
        { "ITRK_DEAD",  0,  0x03, reg_decode_from_table, itrk_dead_desc },
        { NULL }
};

static const char *const dis_rch_desc[] = {
        "recharge enabled",
        "recharge disabled",
};

static const char *const vrch_desc[] = {
        "80 mV",
        "160 mV",
        "200 mV",
        "260 mV",
};

static const char *const vtrk_dead_desc[] = {
        "2.0 V",
        "2.5 V",
        "2.6 V",
        "2.9 V",
};

static const char *const vweak_desc[] = {
        "2.7 V",
        "2.8 V",
        "2.9 V",
        "3.0 V",
        "3.1 V",
        "3.2 V",
        "3.3 V",
        "3.4 V",
};

/* Fields for Voltage thresholds register */
static const struct reg_field adp_reg_vol_thre_fields[] = {
        { "DIS_RCH",    7,  0x80, reg_decode_from_table, dis_rch_desc },
        { "VRCH",       5,  0x60, reg_decode_from_table, vrch_desc },
        { "VTRK_DEAD",  3,  0x18, reg_decode_from_table, vtrk_dead_desc },
        { "VWAEK",      0,  0x07, reg_decode_from_table, vweak_desc },
        { NULL }
};

const char *const en_tend_desc[] = {
        "Charge complete timer disabled",
        "Charge complete timer enabled",
};

const char *const en_ch_timer_desc[] = {
        "Trickle/fast timer disabled",
        "Trickle/fast timer enabled",
};

const char *const chg_tmr_period_desc[] = {
        "30 sec trickle timer and 300 minute fast charge timer",
        "60 sec trickle timer and 600 minute fast charge timer",
};

const char *const en_wd_desc[] = {
        "Watchdog timer is disabled evn when BAT_SNS exceeds Vdead",
        "Watchdog timer safety timer is enabled",
};

const char *const wd_period_desc[] = {
        "32 sec watchdog timer and 40 min safety timer",
        "64 sec watchdog timer and 40 min safety timer",
};

/* Fields for Timer settings register */
static const struct reg_field adp_reg_timer_fields[] = {
        { "EN_TEND",        5,  0x20, reg_decode_from_table, en_tend_desc },
        { "EN_CH_TIMER",    4,  0x10, reg_decode_from_table, en_ch_timer_desc },
        { "CHG_TMR_PERIOD", 3,  0x08, reg_decode_from_table, chg_tmr_period_desc },
        { "EN_WD",          2,  0x04, reg_decode_from_table, en_wd_desc },
        { "WD_PERIOD",      1,  0x02, reg_decode_from_table, wd_period_desc },
        { "RESET_WD",       0,  0x01, write_only_field, NULL },
        { NULL }
};

const char *const dis_ic1_desc[] = {
        "Normal operation",
        "Charger disabled, Vvin must be Viso_b < Vvin < 5.5 V",
};

const char *const en_bmon_desc[] = {
        "when Vvin < Vvin_ok battery monitor is disabled",
        "the battery monitor is enabled even when VINx is below Vvin_ok",
};

const char *const en_thr_desc[] = {
        "when Vvin < Vvin_ok the THR current source is disabled",
        "THR current source is enabled even when VINx is below Vvin_ok",
};

const char *const dis_ldo_desc[] = {
        "LDO is enabled",
        "LDO is off",
};

const char *const en_eoc_desc[] = {
        "end of charge not allowed",
        "end of charge allowed",
};

const char *const en_chg_desc[] = {
        "battery charging is disabled",
        "battery charging is enabled",
};

/* Fields for Functional settings 1 register */
static const struct reg_field adp_reg_func1_fields[] = {
        { "DIS_IC1",        6,  0x40, reg_decode_from_table, dis_ic1_desc },
        { "EN_BMON",        5,  0x20, reg_decode_from_table, en_bmon_desc },
        { "EN_THR",         4,  0x10, reg_decode_from_table, en_thr_desc },
        { "DIS_LDO",        3,  0x08, reg_decode_from_table, dis_ldo_desc },
        { "EN_EOC",         2,  0x04, reg_decode_from_table, en_eoc_desc },
        { "EN_CHG",         0,  0x01, reg_decode_from_table, en_chg_desc },
        { NULL }
};

static const char *const en_jeita_desc[] = {
        "JEITA compliance of the Li-Ion temperature battery charging specification is disabled",
        "JEITA compliance enabled",
};

static const char *const jeita_select_desc[] = {
        "JEITA 1 is selected",
        "JEITA 2 is selected",
};

static const char *const en_chg_vlim_desc[] = {
        "Charging voltage limit disabled",
        "Voltage limit activated. Charging when V drops below Vchg_vlim",
};

static const char *const ideal_diode_desc[] = {
        "Ideal diode operates always when Viso_s < Viso_b",
        "Ideal diode operates when Viso_s < Viso_b and Vbat_sns > Vweak",
        "Ideal diode is disabled",
        "Ideal diode is disabled",
};

static const char *const vsystem_desc[] = {
        "4.3 V",
        "4.4 V",
        "4.5 V",
        "4.6 V",
        "4.7 V",
        "4.8 V",
        "4.9 V",
        "5.0 V",
};

/* Fields for Functional settings 2 register */
static const struct reg_field adp_reg_func2_fields[] = {
        { "EN_JEITA",       7,  0x80, reg_decode_from_table, en_jeita_desc },
        { "JEITA_SELECT",   6,  0x40, reg_decode_from_table, jeita_select_desc },
        { "EN_CHG_VLIM",    5,  0x20, reg_decode_from_table, en_chg_vlim_desc },
        { "IDEAL_DIODE",    3,  0x08, reg_decode_from_table, ideal_diode_desc },
        { "VSYSTEM",        0,  0x07, reg_decode_from_table, vsystem_desc },
        { NULL }
};

static const char *const en_therm_lim_int_desc[] = {
        "Isothermal charging interrupt is disabled",
        "Isothermal charging interrupt is enabled",
};

static const char *const en_wd_int_desc[] = {
        "Watchdog alarm interrupt is disabled",
        "Watchdog alarm interrupt is enabled",
};

static const char *const en_tsd_int_desc[] = {
        "Overtemperature interrupt is disabled",
        "Overtemperature interrupt is enabled",
};

static const char *const en_thr_int_desc[] = {
        "THR temperature thresholds interrupt is disabled",
        "THR temperature thresholds interrupt is enabled",
};

static const char *const en_bat_int_desc[] = {
        "Battery voltage thresholds interrupt is disabled",
        "Battery voltage thresholds interrupt is enabled",
};

static const char *const en_chg_int_desc[] = {
        "Charger mode change interrupt is disabled",
        "Charger mode change interrupt is enabled",
};

static const char *const en_vin_int_desc[] = {
        "VINx pin voltage thresholds interrupt is disabled",
        "VINx pin voltage thresholds interrupt is enabled",
};

/* Fields for Interrupt enable register */
static const struct reg_field adp_reg_int_en_fields[] = {
        { "EN_THERM_LIM_INT", 6,  0x40, reg_decode_from_table, en_therm_lim_int_desc },
        { "EN_WD_INT",        5,  0x20, reg_decode_from_table, en_wd_int_desc },
        { "EN_TSD_INT",       4,  0x10, reg_decode_from_table, en_tsd_int_desc },
        { "EN_THR_INT",       3,  0x08, reg_decode_from_table, en_thr_int_desc },
        { "EN_BAT_INT",       2,  0x04, reg_decode_from_table, en_bat_int_desc },
        { "EN_CHG_INT",       1,  0x02, reg_decode_from_table, en_chg_int_desc },
        { "EN_VIN_INT",       0,  0x01, reg_decode_from_table, en_vin_int_desc },
        { NULL }
};

const char *const vin_ov_desc[] = {
        "VINx pin does not exceed Vvin_ov",
        "VINx pin exceeds Vvin_ov",
};

const char *const vin_ok_desc[] = {
        "VINx pin does not exceed Vvin_ok",
        "VINx pin exceeds Vvin_ok",
};

const char *const vin_ilim_desc[] = {
        "Current is not limited",
        "Current on VINx pin is limited by the high voltage blocking FET and charger is not running at full Ichg",
};

const char *const therm_lim_desc[] = {
        "Current is not limited by temperature",
        "Current on limited by die temperature",
};

const char *const chdone_desc[] = {
        "End of charge not reached",
        "End of charge reached (bit latches)",
};

const char *const charger_status_desc[] = {
        "charger off",
        "trickle charge",
        "fast charge (constant current mode)",
        "fast charge (constant voltage mode)",
        "charge complete",
        "LDO mode",
        "trickle or fast charge timer expired",
        "battery detection",
};

/* Fields for Charger status 1 register */
static const struct reg_field adp_reg_chg_sts1_fields[] = {
        { "VIN_OV",           7,  0x80, reg_decode_from_table, vin_ov_desc },
        { "VIN_OK",           6,  0x40, reg_decode_from_table, vin_ok_desc },
        { "VIN_ILIM",         5,  0x20, reg_decode_from_table, vin_ilim_desc },
        { "THERM_LIM",        4,  0x10, reg_decode_from_table, therm_lim_desc },
        { "CHDONE",           3,  0x08, reg_decode_from_table, chdone_desc },
        { "CHARGER_STATUS",   0,  0x07, reg_decode_from_table, charger_status_desc },
        { NULL }
};

static const char *const thr_status_desc[] = {
        "THR pin off",
        "battery cold",
        "battery cold",
        "battery warm",
        "battery hot",
        "???",
        "???",
        "thermistor OK",
};

static const char *const rch_lim_info_desc[] = {
        "Vbat_sns > Vrch",
        "Vbat_sns < Vrch",
};

static const char *const battery_status_desc[] = {
        "battery monitor off",
        "no battery",
        "Vbat_sns < Vtrk",
        "Vtrk <= Vbat_sns < Vweak",
        "Vbat_sns >= Vweak",
        NULL
};

/* Fields for Charger status 1 register */
static const struct reg_field adp_reg_chg_sts2_fields[] = {
        { "THR_STATUS",       5,  0xE0, reg_decode_from_table, thr_status_desc },
        { "RCH_LIM_INFO",     3,  0x08, reg_decode_from_table, rch_lim_info_desc },
        { "BATTERY_STATUS",   0,  0x07, reg_decode_from_table, battery_status_desc },
        { NULL }
};

static const char *const bat_shr_desc[] = {
        "No short",
        "Battery short detected",
};

static const char *const tsd_130c_desc[] = {
        "No overtemperature (lower)",
        "Overtemperature (lower) fault",
};

static const char *const tsd_140c_desc[] = {
        "No overtemperature",
        "Overtemperature fault",
};

/* Fields for Fault register */
static const struct reg_field adp_reg_fault_fields[] = {
        { "BAT_SHR",          3,  0x08, reg_decode_from_table, bat_shr_desc },
        { "TSD_130C",         1,  0x02, reg_decode_from_table, tsd_130c_desc },
        { "TSD_140C",         0,  0x01, reg_decode_from_table, tsd_140c_desc },
        { NULL }
};

static const char *const tbat_shr_desc[] = {
        "1 sec.",
        "2 sec.",
        "4 sec.",
        "10 sec.",
        "30 sec.",
        "60 sec.",
        "120 sec.",
        "180 sec.",
};

static const char *const vbat_shr_desc[] = {
        "2.0 V",
        "2.1 V",
        "2.2 V",
        "2.3 V",
        "2.4 V",
        "2.5 V",
        "2.6 V",
        "2.7 V",
};

/* Fields for Battery short register */
static const struct reg_field adp_reg_bat_shrt_fields[] = {
        { "TBAT_SHR",         5,  0xE0, reg_decode_from_table, tbat_shr_desc },
        { "VBAT_SHR",         0,  0x07, reg_decode_from_table, vbat_shr_desc },
        { NULL }
};

static const char *const iend_desc[] = {
        "12.5 mA",
        "32.5 mA",
        "52.5 mA",
        "72.5 mA",
        "92.5 mA",
        "117.5 mA",
        "142.5 mA",
        "170.0 mA",
};

static const char *const c_20_eoc_desc[] = {
        "0",
        "1",
};

static const char *const c_10_eoc_desc[] = {
        "0",
        "1",
};

static const char *const c_5_eoc_desc[] = {
        "0",
        "1",
};

static const char *const sys_en_set_desc[] = {
        "SYS_EN is activated when LDO is active and system voltage is available",
        "SYS_EN is activated by ISO_Bx, the battery charging mode",
        "SYS_EN is activated and the isolation FET is disabled when the battery drops below Vweak",
        "SYS_EN is activated in LDO mode when the charger is disabled. SYS_EN is activate in the charging mode when Viso_b > Vweak",
};

/* Fields for IEND register */
static const struct reg_field adp_reg_iend_fields[] = {
        { "IEND",             5,  0xE0, reg_decode_from_table, iend_desc },
        { "C_20_EOC",         4,  0x10, reg_decode_from_table, c_20_eoc_desc },
        { "C_10_EOC",         3,  0x08, reg_decode_from_table, c_10_eoc_desc },
        { "C_5_EOC",          2,  0x04, reg_decode_from_table, c_5_eoc_desc },
        { "SYS_EN_SET",       0,  0x03, reg_decode_from_table, sys_en_set_desc },
        { NULL }
};

#else
#define adp_reg_vinx_fields             NULL
#define adp_reg_term_fields             NULL
#define adp_reg_chrg_curr_fields        NULL
#define adp_reg_vol_thre_fields         NULL
#define adp_reg_timer_fields            NULL
#define adp_reg_func1_fields            NULL
#define adp_reg_func2_fields            NULL
#define adp_reg_int_en_fields           NULL
#define adp_reg_chg_sts1_fields         NULL
#define adp_reg_chg_sts2_fields         NULL
#define adp_reg_fault_fields            NULL
#define adp_reg_bat_shrt_fields         NULL
#define adp_reg_iend_fields             NULL
#endif

/**
* Definition of all Register names
*/
static const struct adp5061_reg adp5061_device_regs[] = {
    {.addr = REG_PART_ID,           .name = "PART_ID"               },
    {.addr = REG_SILICON_REV,       .name = "SILICON_REV"           },
    {.addr = REG_VIN_PIN_SETTINGS,  .name = "VINx_PIN_SETTINGS",
            .fields = adp_reg_vinx_fields                           },
    {.addr = REG_TERM_SETTINGS,     .name = "TERMINATION_SETTINGS",
            .fields = adp_reg_term_fields                           },
    {.addr = REG_CHARGING_CURRENT,  .name = "CHARGING_CURRENT",
            .fields = adp_reg_chrg_curr_fields                      },
    {.addr = REG_VOLTAGE_THRES,     .name = "VOLTAGE_THRESHOLD",
            .fields = adp_reg_vol_thre_fields                       },
    {.addr = REG_TIMER_SETTINGS,    .name = "TIMER_SETTINGS",
            .fields = adp_reg_timer_fields                          },
    {.addr = REG_FUNC_SETTINGS_1,   .name = "FUNCTIONAL_SETTINGS_1",
            .fields = adp_reg_func1_fields                          },
    {.addr = REG_FUNC_SETTINGS_2,   .name = "FUNCTIONAL_SETTINGS_2",
            .fields = adp_reg_func2_fields                          },
    {.addr = REG_INT_EN,            .name = "INTERRUPT_EN",
            .fields = adp_reg_int_en_fields                         },
    {.addr = REG_INT_ACTIVE,        .name = "INTERRUPT_ACTIVE"      },
    {.addr = REG_CHARGER_STATUS_1,  .name = "CHARGER_STATUS_1",
            .fields = adp_reg_chg_sts1_fields                       },
    {.addr = REG_CHARGER_STATUS_2,  .name = "CHARGER_STATUS_2",
            .fields = adp_reg_chg_sts2_fields                       },
    {.addr = REG_FAULT_REGISTER,    .name = "FAULT_REGISTER",
            .fields = adp_reg_fault_fields                          },
    {.addr = REG_BATT_SHORT,        .name = "BATTERY_SHORT",
            .fields = adp_reg_bat_shrt_fields                       },
    {.addr = REG_IEND,              .name = "IEND",
            .fields = adp_reg_iend_fields                           },

};
#define NUM_DEVICE_REGS (sizeof(adp5061_device_regs)\
                        /sizeof(adp5061_device_regs[0]))

static int adp5061_shell_cmd(int argc, char **argv);

static const struct shell_cmd adp5061_shell_cmd_struct = {
    .sc_cmd = "adp5061",
    .sc_cmd_func = adp5061_shell_cmd
};

static int
adp5061_shell_err_too_many_args(const char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return SYS_EINVAL;
}

static int
adp5061_shell_err_unknown_arg(const char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return SYS_EINVAL;
}

static int
adp5061_shell_err_invalid_arg(const char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return SYS_EINVAL;
}

static int
adp5061_shell_err_missing_arg(const char *cmd_name)
{
    console_printf("Error: missing %s\n", cmd_name);
    return SYS_EINVAL;
}

static int
adp5061_shell_help(void)
{
    console_printf("%s cmd\n", adp5061_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\thelp\n");
#if MYNEWT_VAL(ADP5061_CLI_DECODE)
    console_printf("\tdump [decode]\n");
#else
    console_printf("\tdump\n");
#endif
    console_printf("\tread reg\n");
    console_printf("\twrite reg\n");
    console_printf("\tdisable\n");
    console_printf("\tenable\n");
    console_printf("\tstatus\n");

    return 0;
}

#if MYNEWT_VAL(ADP5061_CLI_DECODE)
static const char *
field_bin_value(const struct reg_field *field, uint8_t val, char *buf)
{
    uint32_t mask = field->fld_mask >> field->fld_pos;
    int bits = 0;
    val = (val & field->fld_mask) >> field->fld_pos;
    for (bits = 0; mask; bits++) {
        mask >>= 1;
    }
    mask = field->fld_mask >> field->fld_pos;
    buf[bits] = '\0';
    while (mask) {
        buf[--bits] = '0' + (val & 1);
        mask >>= 1;
        val >>= 1;
    }
    return buf;
}
#endif

static int
adp5061_shell_cmd_dump(int argc, char **argv)
{
    uint8_t val;
    int i;
#if MYNEWT_VAL(ADP5061_CLI_DECODE)
    int decode = 0;
    const struct reg_field *field;
    char binary[9];
    char buf[20];
#endif
    const struct adp5061_reg *reg;

#if MYNEWT_VAL(ADP5061_CLI_DECODE)
    if (argc > 3) {
        return adp5061_shell_err_too_many_args(argv[1]);
    } else if (argc == 3) {
        if (strcmp(argv[2], "decode") == 0) {
            decode = 1;
        } else {
            return adp5061_shell_err_unknown_arg(argv[2]);
        }
    }
#else
    if (argc > 2) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }
#endif

    console_printf("========== Device Regs ==========\n");
    console_printf("==========   ADP5061   ==========\n");
    console_printf("=================================\n\n");
    for (i = 0; i < NUM_DEVICE_REGS; ++i) {
        reg = &adp5061_device_regs[i];
        if (adp5061_get_reg(adp5061_dev,
                reg->addr, &val)) {
            console_printf("%21s [0x%02x] = READ ERROR \n",
                    reg->name, reg->addr);
        } else {
            console_printf("%21s [0x%02x] = 0x%02x \n",
                    reg->name,
                    reg->addr, val);
#if MYNEWT_VAL(ADP5061_CLI_DECODE)
            if (decode && reg->fields)
            {
                for (field = reg->fields; field->fld_name; ++field)
                {
                    console_printf("%28s = %s %s\n", field->fld_name,
                                   field_bin_value(field, val, binary),
                                   field->fld_decode_value(field, val, buf));
                }
            }
#endif
        }
    }

    return 0;
}

static int
adp5061_shell_read_reg(int argc, char **argv)
{
    uint8_t reg_val;
    uint8_t reg_ix;
    int i;
    int status;

    if (argc < 3) {
        return adp5061_shell_err_missing_arg("register name of address");
    }
    if (argc > 3) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }

    reg_ix = (uint8_t)parse_ull_bounds(argv[2], REG_PART_ID, REG_IEND,
            &status);

    for (i = 0; i < NUM_DEVICE_REGS; ++i) {
        if ((status == 0 && adp5061_device_regs[i].addr == reg_ix) ||
            (strcmp(adp5061_device_regs[i].name, argv[2]) == 0)) {
            if (adp5061_get_reg(adp5061_dev,
                    adp5061_device_regs[i].addr, &reg_val)) {
                console_printf("%21s [0x%02x] = READ ERROR \n",
                        adp5061_device_regs[i].name,
                        adp5061_device_regs[i].addr);
            } else {
                console_printf("%21s [0x%02x] = 0x%02x \n",
                        adp5061_device_regs[i].name,
                        adp5061_device_regs[i].addr, reg_val);
            }
            return 0;
        }
    }
    adp5061_shell_err_invalid_arg(argv[2]);

    return 0;
}

static int
adp5061_shell_write_reg(int argc, char **argv)
{
    uint8_t reg_val;
    uint8_t reg_ix;
    int i;
    int status;
    int rc;

    if (argc < 3) {
        return adp5061_shell_err_missing_arg("register name of address");
    }
    if (argc < 4) {
        return adp5061_shell_err_missing_arg("register value");
    }
    if (argc > 4) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }

    reg_val = (uint8_t)parse_ull_bounds(argv[3], 0, 0xFF, &status);
    if (status) {
        console_printf("Invalid register value\n");
        return status;
    }
    reg_ix = (uint8_t)parse_ull_bounds(argv[2], REG_PART_ID, REG_IEND,
            &status);

    for (i = 0; i < NUM_DEVICE_REGS; ++i) {
        if ((status == 0 && adp5061_device_regs[i].addr == reg_ix) ||
            (strcmp(adp5061_device_regs[i].name, argv[2]) == 0)) {
            rc = adp5061_set_reg(adp5061_dev, adp5061_device_regs[i].addr,
                    reg_val);
            if (rc) {
                console_printf("Write register failed 0x%X\n", rc);
            } else {
                console_printf("Register written\n");
            }
            return 0;
        }
    }
    adp5061_shell_err_invalid_arg(argv[2]);

    return 0;
}

static int
adp5061_shell_enable(int argc, char **argv)
{
    int rc;

    if (argc > 2) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }

    rc = adp5061_charge_enable(adp5061_dev);
    if (rc) {
        console_printf("Charger enable failed 0x%X\n", rc);
    } else {
        console_printf("Charger enabled\n");
    }

    return 0;
}

static int
adp5061_shell_disable(int argc, char **argv)
{
    int rc;

    if (argc > 2) {
        return adp5061_shell_err_too_many_args(argv[1]);
    }

    rc = adp5061_charge_disable(adp5061_dev);
    if (rc) {
        console_printf("Charger disable failed 0x%X\n", rc);
    } else {
        console_printf("Charger disabled\n");
    }

    return 0;
}

static int
adp5061_shell_charger_status(int argc, char **argv)
{
    int rc = 0;
    uint8_t status_1;
    uint8_t status_2;

    if (argc > 2) {
        rc = adp5061_shell_err_too_many_args(argv[1]);
        goto err;
    }

    rc = adp5061_get_reg(adp5061_dev, REG_CHARGER_STATUS_1, &status_1);
    if (rc) {
        console_printf("Read charger status 1 failed %d\n", rc);
        goto err;
    }
#if MYNEWT_VAL(ADP5061_CLI_DECODE)
    console_printf("CHARGER_STATUS: %s\n",
        charger_status_desc[ADP5061_CHG_STATUS_1_GET(status_1)]);
#else
    console_printf("CHARGER_STATUS: %d\n",
            ADP5061_CHG_STATUS_1_GET(status_1));
#endif
    console_printf("VIN_OK: %d\n",
            ADP5061_CHG_STATUS_1_VIN_OK_GET(status_1));
    console_printf("VIN_OV: %d\n",
            ADP5061_CHG_STATUS_1_VIN_OV_GET(status_1));
    console_printf("VIN_ILIM: %d\n",
            ADP5061_CHG_STATUS_1_VIN_ILIM_GET(status_1));
    console_printf("THERM_ILIM: %d\n",
            ADP5061_CHG_STATUS_1_THERM_LIM_GET(status_1));
    console_printf("CHDONE: %d\n",
            ADP5061_CHG_STATUS_1_CHDONE_GET(status_1));

    rc = adp5061_get_reg(adp5061_dev, REG_CHARGER_STATUS_2, &status_2);
    if (rc) {
        console_printf("Read charger status 1 failed %d\n", rc);
        goto err;
    }
#if MYNEWT_VAL(ADP5061_CLI_DECODE)
    console_printf("THR_STATUS: %s\n",
            thr_status_desc[ADP5061_CHG_STATUS_2_THR_GET(status_2)]);
#else
    console_printf("THR_STATUS: %d\n",
            ADP5061_CHG_STATUS_2_THR_GET(status_2));
#endif
    console_printf("RCH_LIM_INFO: %d\n",
            ADP5061_CHG_STATUS_2_RCH_LIM_GET(status_2));
#if MYNEWT_VAL(ADP5061_CLI_DECODE)
    console_printf("BATTERY_STATUS: %s\n",
            battery_status_desc[ADP5061_CHG_STATUS_2_BATT_GET(status_2)]);
#else
    console_printf("BATTERY_STATUS: %d\n",
            ADP5061_CHG_STATUS_2_BATT_GET(status_2));
#endif
err:
    return rc;
}

static int
adp5061_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return adp5061_shell_help();
    }

    if (strcmp(argv[1], "help") == 0) {
        return adp5061_shell_help();
    } else if (strcmp(argv[1], "dump") == 0) {
        return adp5061_shell_cmd_dump(argc, argv);
    } else if (strcmp(argv[1], "read") == 0) {
        return adp5061_shell_read_reg(argc, argv);
    } else if (strcmp(argv[1], "write") == 0) {
        return adp5061_shell_write_reg(argc, argv);
    } else if (strcmp(argv[1], "enable") == 0) {
        return adp5061_shell_enable(argc, argv);
    } else if (strcmp(argv[1], "disable") == 0) {
        return adp5061_shell_disable(argc, argv);
    } else if (strcmp(argv[1], "status") == 0) {
        return adp5061_shell_charger_status(argc, argv);
    }

    return adp5061_shell_err_unknown_arg(argv[1]);
}

int
adp5061_shell_init(struct adp5061_dev *dev)
{
    int rc;

    adp5061_dev = dev;
    rc = shell_cmd_register(&adp5061_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
