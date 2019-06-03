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

#include <os/mynewt.h>

#if MYNEWT_VAL(DA1469X_CHARGER_CLI)

#include <console/console.h>
#include <shell/shell.h>
#include <da1469x_charger/da1469x_charger.h>
#include <parse/parse.h>
#include <stdio.h>
#include <DA1469xAB.h>
#if MYNEWT_VAL(SDADC_BATTERY)
#include <sdadc_da1469x/sdadc_da1469x.h>
#endif
#if MYNEWT_VAL(GPADC_BATTERY)
#include <gpadc_da1469x/gpadc_da1469x.h>
#endif

#if MYNEWT_VAL(SHELL_CMD_HELP)
#define HELP(a) &(da1469x_charger_##a##_help)
static const struct shell_cmd_help da1469x_charger_dump_help = {
    .summary = "Displays charger related registers",
#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)
    .usage = "dump [decode]",
#else
    .usage = NULL,
#endif
    .params = NULL,
};

#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)
static const struct shell_cmd_help da1469x_charger_decode_help = {
    .summary = "Enables or disables decoding of registers",
    .usage = "decode 1 | 0",
    .params = NULL,
};
#endif

static const struct shell_cmd_help da1469x_charger_enable_help = {
    .summary = "Enables charging",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help da1469x_charger_disable_help = {
    .summary = "Disables charging",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help da1469x_charger_status_help = {
    .summary = "Shows status of charger and battery",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help da1469x_charger_clear_irq_help = {
    .summary = "Clears interrupts",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help da1469x_charger_set_i_help = {
    .summary = "Sets charging currents",
    .usage = "seti <charge_current> [precharge_current [eoc percentage]]",
    .params = NULL,
};

static const struct shell_cmd_help da1469x_charger_set_v_help = {
    .summary = "Sets charging voltages",
    .usage = "setv <charge_v> [<precharge_v> [<replenish_v> [ovp_v]]]",
    .params = NULL,
};

#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
static const struct shell_cmd_help da1469x_charger_listen_help = {
    .summary = "Starts or stops charging state notifications",
    .usage = "listen start | stop",
    .params = NULL,
};
#endif

static const struct shell_cmd_help da1469x_charger_reg_help = {
    .summary = "Read or writes register",
    .usage = "<reg_name> ",
    .params = NULL,
};

#else
#define HELP(a) NULL
#endif

/*
 * Struct for field name description.
 * Used for argument validation and field decoding
 */
struct reg_field {
    /* Name of field */
    const char *fld_name;
    /* Position of field in register (LSB position) */
    uint8_t fld_pos;
    bool fld_show_bits;
    /* Mask of bit field to set or extract */
    uint32_t fld_mask;
    /* Function that will convert register value to descriptive string */
    const char *(*fld_decode_value)(const struct reg_field *field,
            uint32_t reg_val, char *buf);
    /* Argument that could be used by above function */
    const void *fld_arg;
};

/**
* Register structure
*/
struct reg {
    /* Address of the register */
    volatile uint32_t *addr;
    /* Name of the register */
    const char *name;
    /* Array of bit fields */
    const struct reg_field *fields;
};

static const char *const charger_state[] = {
    "POWER_UP",
    "INIT",
    "DISABLED",
    "PRE_CHARGE",
    "CC_CHARGE",
    "CV_CHARGE",
    "END_OF_CHARGE",
    "TDIE_PROT",
    "TBAT_PROT",
    "BYPASSED",
    "ERROR",
    NULL,
};

static const char *const tbat_status[] = {
    "",
    "COLD",
    "COOL",
    "",
    "NORMAL",
    "",
    "",
    "",
    "WARM",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "HOT",
    NULL,
};

#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)

bool da1469x_charger_cli_decode_fields;

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
                      uint32_t reg_val, char *buf)
{
    uint32_t val = (reg_val & field->fld_mask) >> field->fld_pos;
    const char *const *table = field->fld_arg;

    return strcpy(buf, val_decode_from_table(table, val));
}

#define FIELD_NUM(reg, field) { .fld_name = #field, .fld_pos = CHARGER_CHARGER_##reg##_REG_##field##_Pos, \
    .fld_show_bits = 0, .fld_mask = CHARGER_CHARGER_##reg##_REG_##field##_Msk }
#define FIELD_TAB(reg, field, tab) { .fld_name = #field, .fld_pos = CHARGER_CHARGER_##reg##_REG_##field##_Pos, \
    .fld_show_bits = 1, .fld_mask = CHARGER_CHARGER_##reg##_REG_##field##_Msk, .fld_decode_value = reg_decode_from_table, .fld_arg = tab }
#define FIELD_FUN(reg, field, fun, arg) { .fld_name = #field, .fld_pos = CHARGER_CHARGER_##reg##_REG_##field##_Pos, \
    .fld_show_bits = 1, .fld_mask = CHARGER_CHARGER_##reg##_REG_##field##_Msk, .fld_decode_value = fun, .fld_arg = arg }

/* Fields for CTRL register */
static const struct reg_field CTRL_fields[] = {
    FIELD_NUM(CTRL, EOC_INTERVAL_CHECK_TIMER),
    FIELD_NUM(CTRL, EOC_INTERVAL_CHECK_THRES),
    FIELD_NUM(CTRL, REPLENISH_MODE),
    FIELD_NUM(CTRL, PRE_CHARGE_MODE),
    FIELD_NUM(CTRL, CHARGE_LOOP_HOLD),
    FIELD_NUM(CTRL, JEITA_SUPPORT_DISABLED),
    FIELD_NUM(CTRL, TBAT_MONITOR_MODE),
    FIELD_NUM(CTRL, CHARGE_TIMERS_HALT_ENABLE),
    FIELD_NUM(CTRL, NTC_LOW_DISABLE),
    FIELD_NUM(CTRL, TBAT_PROT_ENABLE),
    FIELD_NUM(CTRL, TDIE_ERROR_RESUME),
    FIELD_NUM(CTRL, TDIE_PROT_ENABLE),
    FIELD_NUM(CTRL, CHARGER_RESUME),
    FIELD_NUM(CTRL, CHARGER_BYPASS),
    FIELD_NUM(CTRL, CHARGE_START),
    FIELD_NUM(CTRL, CHARGER_ENABLE),
    { NULL }
};

static const char *const charger_jeita_state[] = {
    "CHECK_IDLE",
    "CHECK_THOT",
    "CHECK_TCOLD",
    "CHECK_TWORM",
    "CHECK_TCOOL",
    "CHECK_TNORMAL",
    "UPDATE_TBAT",
    NULL,
};

/* Fields for STATUS register */
static const struct reg_field STATUS_fields[] = {
    FIELD_NUM(STATUS, OVP_EVENTS_DEBOUNCE_CNT),
    FIELD_NUM(STATUS, EOC_EVENTS_DEBOUNCE_CNT),
    FIELD_NUM(STATUS, TDIE_ERROR_DEBOUNCE_CNT),
    FIELD_TAB(STATUS, CHARGER_JEITA_STATE, charger_jeita_state),
    FIELD_TAB(STATUS, CHARGER_STATE, charger_state),
    FIELD_TAB(STATUS, TBAT_STATUS, tbat_status),
    FIELD_NUM(STATUS, MAIN_TBAT_COMP_OUT),
    FIELD_NUM(STATUS, TBAT_HOT_COMP_OUT),
    FIELD_NUM(STATUS, TDIE_COMP_OUT),
    FIELD_NUM(STATUS, VBAT_OVP_COMP_OUT),
    FIELD_NUM(STATUS, MAIN_VBAT_COMP_OUT),
    FIELD_NUM(STATUS, END_OF_CHARGE),
    FIELD_NUM(STATUS, CHARGER_CV_MODE),
    FIELD_NUM(STATUS, CHARGER_CC_MODE),
    FIELD_NUM(STATUS, CHARGER_IS_POWERED_UP),
    { NULL }
};

static const char *
voltage_decode(const struct reg_field *field, uint32_t reg_val, char *buf)
{
    uint32_t val = (reg_val & field->fld_mask) >> field->fld_pos;

    if (val < 20) {
        val = 280 + 5 * val;
    } else if (val < 60) {
        val = 380 + 2 * (val - 20);
    } else {
        val = 460 + 10 * (val - 60);
    }
    sprintf(buf, "%d.%02d V", (int)val / 100, (int)val % 100);
    return buf;
}

/* Fields for VOLTAGE_PARAM register */
static const struct reg_field VOLTAGE_PARAM_fields[] = {
    FIELD_FUN(VOLTAGE_PARAM, V_OVP, voltage_decode, NULL),
    FIELD_FUN(VOLTAGE_PARAM, V_REPLENISH, voltage_decode, NULL),
    FIELD_FUN(VOLTAGE_PARAM, V_PRECHARGE, voltage_decode, NULL),
    FIELD_FUN(VOLTAGE_PARAM, V_CHARGE, voltage_decode, NULL),
    { NULL }
};

static const uint16_t i_eoc[][8] = {
    { 40, 55, 70, 85, 100, 120, 140, 160},
    { 88, 121, 154, 187, 220, 264, 308, 352},
};

static const char *
current_eoc_decode(const struct reg_field *field, uint32_t reg_val, char *buf)
{
    int val;
    size_t ix = (reg_val & field->fld_mask) >> field->fld_pos;
    size_t mul = 0;

    if (reg_val & CHARGER_CHARGER_CURRENT_PARAM_REG_I_EOC_DOUBLE_RANGE_Msk) {
        mul = 1;
    }
    val = i_eoc[mul][ix];

    sprintf(buf, "%d.%d %%", val / 10, val % 10);

    return buf;
}

static const char *
current_precharge(const struct reg_field *field, uint32_t reg_val, char *buf)
{
    int val = (int)((reg_val & field->fld_mask) >> field->fld_pos);

    if (val < 15) {
        val = 5 + 5 * val;
    } else if (val < 32) {
        val = 80 + 10 * (val - 15);
    } else if (val < 47) {
        val = 240 + 20 * (val - 31);
    } else {
        val = 560;
    }
    sprintf(buf, "%d.%d mA", val / 10, val % 10);

    return buf;
}

static const char *
current_charge(const struct reg_field *field, uint32_t reg_val, char *buf)
{
    int val = (int)((reg_val & field->fld_mask) >> field->fld_pos);

    if (val < 15) {
        val = 5 + 5 * val;
    } else if (val < 32) {
        val = 80 + 10 * (val - 15);
    } else if (val < 47) {
        val = 240 + 20 * (val - 31);
    } else {
        val = 560;
    }
    sprintf(buf, "%d mA", val);

    return buf;
}

/* Fields for CURRENT_PARAM register */
static const struct reg_field CURRENT_PARAM_fields[] = {
    FIELD_NUM(CURRENT_PARAM, I_EOC_DOUBLE_RANGE),
    FIELD_FUN(CURRENT_PARAM, I_END_OF_CHARGE, current_eoc_decode, NULL),
    FIELD_FUN(CURRENT_PARAM, I_PRECHARGE, current_precharge, NULL),
    FIELD_FUN(CURRENT_PARAM, I_CHARGE, current_charge, NULL),
    { NULL }
};

static const char *const tdie_max[] = {
    "0 C",
    "50 C",
    "80 C",
    "90 C",
    "100 C",
    "110 C",
    "120 C",
    "130 C",
    NULL,
};

static const char *
tbat_temp(const struct reg_field *field, uint32_t reg_val, char *buf)
{
    int val = (int)((reg_val & field->fld_mask) >> field->fld_pos);

    val -= 10;
    sprintf(buf, "%d C", val);
    return buf;
}

/* Fields for TEMPSET_PARAM register */
static const struct reg_field TEMPSET_PARAM_fields[] = {
    FIELD_TAB(TEMPSET_PARAM, TDIE_MAX, tdie_max),
    FIELD_FUN(TEMPSET_PARAM, TBAT_HOT, tbat_temp, NULL),
    FIELD_FUN(TEMPSET_PARAM, TBAT_WARM, tbat_temp, NULL),
    FIELD_FUN(TEMPSET_PARAM, TBAT_COOL, tbat_temp, NULL),
    FIELD_FUN(TEMPSET_PARAM, TBAT_COLD, tbat_temp, NULL),
    { NULL }
};

/* Fields for PRE_CHARGE_TIMER register */
static const struct reg_field PRE_CHARGE_TIMER_fields[] = {
    FIELD_NUM(PRE_CHARGE_TIMER, PRE_CHARGE_TIMER),
    FIELD_NUM(PRE_CHARGE_TIMER, MAX_PRE_CHARGE_TIME),
    { NULL }
};

/* Fields for CC_CHARGE_TIMER register */
static const struct reg_field CC_CHARGE_TIMER_fields[] = {
    FIELD_NUM(CC_CHARGE_TIMER, CC_CHARGE_TIMER),
    FIELD_NUM(CC_CHARGE_TIMER, MAX_CC_CHARGE_TIME),
    { NULL }
};

/* Fields for CV_CHARGE_TIMER register */
static const struct reg_field CV_CHARGE_TIMER_fields[] = {
    FIELD_NUM(CV_CHARGE_TIMER, CV_CHARGE_TIMER),
    FIELD_NUM(CV_CHARGE_TIMER, MAX_CV_CHARGE_TIME),
    { NULL }
};

/* Fields for TOTAL_CHARGE_TIMER register */
static const struct reg_field TOTAL_CHARGE_TIMER_fields[] = {
    FIELD_NUM(TOTAL_CHARGE_TIMER, TOTAL_CHARGE_TIMER),
    FIELD_NUM(TOTAL_CHARGE_TIMER, MAX_TOTAL_CHARGE_TIME),
    { NULL }
};

/* Fields for JEITA_V_CHARGE register */
static const struct reg_field JEITA_V_CHARGE_fields[] = {
    FIELD_FUN(JEITA_V_CHARGE, V_CHARGE_TWARM, voltage_decode, NULL),
    FIELD_FUN(JEITA_V_CHARGE, V_CHARGE_TCOOL, voltage_decode, NULL),
    { NULL }
};

/* Fields for JEITA_V_PRECHARGE register */
static const struct reg_field JEITA_V_PRECHARGE_fields[] = {
    FIELD_FUN(JEITA_V_PRECHARGE, V_PRECHARGE_TWARM, voltage_decode, NULL),
    FIELD_FUN(JEITA_V_PRECHARGE, V_PRECHARGE_TCOOL, voltage_decode, NULL),
    { NULL }
};

/* Fields for JEITA_V_REPLENISH register */
static const struct reg_field JEITA_V_REPLENISH_fields[] = {
    FIELD_FUN(JEITA_V_REPLENISH, V_REPLENISH_TWARM, voltage_decode, NULL),
    FIELD_FUN(JEITA_V_REPLENISH, V_REPLENISH_TCOOL, voltage_decode, NULL),
    { NULL }
};

/* Fields for JEITA_V_OVP register */
static const struct reg_field JEITA_V_OVP_fields[] = {
    FIELD_FUN(JEITA_V_OVP, V_OVP_TWARM, voltage_decode, NULL),
    FIELD_FUN(JEITA_V_OVP, V_OVP_TCOOL, voltage_decode, NULL),
    { NULL }
};

/* Fields for JEITA_CURRENT register */
static const struct reg_field JEITA_CURRENT_fields[] = {
    FIELD_FUN(JEITA_CURRENT, I_PRECHARGE_TWARM, current_precharge, NULL),
    FIELD_FUN(JEITA_CURRENT, I_PRECHARGE_TCOOL, current_precharge, NULL),
    FIELD_FUN(JEITA_CURRENT, I_CHARGE_TWARM, current_charge, NULL),
    FIELD_FUN(JEITA_CURRENT, I_CHARGE_TCOOL, current_charge, NULL),
    { NULL }
};

/* Fields for VBAT_COMP_TIMER register */
static const struct reg_field VBAT_COMP_TIMER_fields[] = {
    FIELD_NUM(VBAT_COMP_TIMER, VBAT_COMP_TIMER),
    FIELD_NUM(VBAT_COMP_TIMER, VBAT_COMP_SETTLING),
    { NULL }
};

/* Fields for VOVP_COMP_TIMER register */
static const struct reg_field VOVP_COMP_TIMER_fields[] = {
    FIELD_NUM(VOVP_COMP_TIMER, OVP_INTERVAL_CHECK_TIMER),
    FIELD_NUM(VOVP_COMP_TIMER, VBAT_OVP_COMP_TIMER),
    FIELD_NUM(VOVP_COMP_TIMER, OVP_INTERVAL_CHECK_THRES),
    FIELD_NUM(VOVP_COMP_TIMER, VBAT_OVP_COMP_SETTLING),
    { NULL }
};

/* Fields for TDIE_COMP_TIMER register */
static const struct reg_field TDIE_COMP_TIMER_fields[] = {
    FIELD_NUM(TDIE_COMP_TIMER, TDIE_COMP_TIMER),
    FIELD_NUM(TDIE_COMP_TIMER, TDIE_COMP_SETTLING),
    { NULL }
};

/* Fields for TBAT_MON_TIMER register */
static const struct reg_field TBAT_MON_TIMER_fields[] = {
    FIELD_NUM(TBAT_MON_TIMER, TBAT_MON_TIMER),
    FIELD_NUM(TBAT_MON_TIMER, TBAT_MON_INTERVAL),
    { NULL }
};

/* Fields for TBAT_COMP_TIMER register */
static const struct reg_field TBAT_COMP_TIMER_fields[] = {
    FIELD_NUM(TBAT_COMP_TIMER, TBAT_COMP_TIMER),
    FIELD_NUM(TBAT_COMP_TIMER, TBAT_COMP_SETTLING),
    { NULL }
};

/* Fields for THOT_COMP_TIMER register */
static const struct reg_field THOT_COMP_TIMER_fields[] = {
    FIELD_NUM(THOT_COMP_TIMER, THOT_COMP_TIMER),
    FIELD_NUM(THOT_COMP_TIMER, THOT_COMP_SETTLING),
    { NULL }
};

/* Fields for PWR_UP_TIMER register */
static const struct reg_field PWR_UP_TIMER_fields[] = {
    FIELD_NUM(PWR_UP_TIMER, CHARGER_PWR_UP_TIMER),
    FIELD_NUM(PWR_UP_TIMER, CHARGER_PWR_UP_SETTLING),
    { NULL }
};

/* Fields for STATE_IRQ_MASK register */
static const struct reg_field STATE_IRQ_MASK_fields[] = {
    FIELD_NUM(STATE_IRQ_MASK, CV_TO_PRECHARGE_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, CC_TO_PRECHARGE_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, CV_TO_CC_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, TBAT_STATUS_UPDATE_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, TBAT_PROT_TO_PRECHARGE_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, TDIE_PROT_TO_PRECHARGE_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, EOC_TO_PRECHARGE_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, CV_TO_EOC_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, CC_TO_EOC_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, CC_TO_CV_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, PRECHARGE_TO_CC_IRQ_EN),
    FIELD_NUM(STATE_IRQ_MASK, DISABLED_TO_PRECHARGE_IRQ_EN),
    { NULL }
};

/* Fields for ERROR_IRQ_MASK register */
static const struct reg_field ERROR_IRQ_MASK_fields[] = {
    FIELD_NUM(ERROR_IRQ_MASK, TBAT_ERROR_IRQ_EN),
    FIELD_NUM(ERROR_IRQ_MASK, TDIE_ERROR_IRQ_EN),
    FIELD_NUM(ERROR_IRQ_MASK, VBAT_OVP_ERROR_IRQ_EN),
    FIELD_NUM(ERROR_IRQ_MASK, TOTAL_CHARGE_TIMEOUT_IRQ_EN),
    FIELD_NUM(ERROR_IRQ_MASK, CV_CHARGE_TIMEOUT_IRQ_EN),
    FIELD_NUM(ERROR_IRQ_MASK, CC_CHARGE_TIMEOUT_IRQ_EN),
    FIELD_NUM(ERROR_IRQ_MASK, PRECHARGE_TIMEOUT_IRQ_EN),
    { NULL }
};

/* Fields for STATE_IRQ_STATUS register */
static const struct reg_field STATE_IRQ_STATUS_fields[] = {
    FIELD_NUM(STATE_IRQ_STATUS, CV_TO_PRECHARGE_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, CC_TO_PRECHARGE_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, CV_TO_CC_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, TBAT_STATUS_UPDATE_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, TBAT_PROT_TO_PRECHARGE_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, TDIE_PROT_TO_PRECHARGE_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, EOC_TO_PRECHARGE_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, CV_TO_EOC_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, CC_TO_EOC_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, CC_TO_CV_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, PRECHARGE_TO_CC_IRQ),
    FIELD_NUM(STATE_IRQ_STATUS, DISABLED_TO_PRECHARGE_IRQ),
    { NULL }
};

/* Fields for ERROR_IRQ_STATUS register */
static const struct reg_field ERROR_IRQ_STATUS_fields[] = {
    FIELD_NUM(ERROR_IRQ_STATUS, TBAT_ERROR_IRQ),
    FIELD_NUM(ERROR_IRQ_STATUS, TDIE_ERROR_IRQ),
    FIELD_NUM(ERROR_IRQ_STATUS, VBAT_OVP_ERROR_IRQ),
    FIELD_NUM(ERROR_IRQ_STATUS, TOTAL_CHARGE_TIMEOUT_IRQ),
    FIELD_NUM(ERROR_IRQ_STATUS, CV_CHARGE_TIMEOUT_IRQ),
    FIELD_NUM(ERROR_IRQ_STATUS, CC_CHARGE_TIMEOUT_IRQ),
    FIELD_NUM(ERROR_IRQ_STATUS, PRECHARGE_TIMEOUT_IRQ),
    { NULL }
};

/* Fields for STATE_IRQ_CLR register */
static const struct reg_field STATE_IRQ_CLR_fields[] = {
    FIELD_NUM(STATE_IRQ_CLR, CV_TO_PRECHARGE_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, CC_TO_PRECHARGE_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, CV_TO_CC_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, TBAT_STATUS_UPDATE_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, TBAT_PROT_TO_PRECHARGE_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, TDIE_PROT_TO_PRECHARGE_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, EOC_TO_PRECHARGE_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, CV_TO_EOC_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, CC_TO_EOC_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, CC_TO_CV_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, PRECHARGE_TO_CC_IRQ_CLR),
    FIELD_NUM(STATE_IRQ_CLR, DISABLED_TO_PRECHARGE_IRQ_CLR),
    { NULL }
};

/* Fields for ERROR_IRQ_CLR register */
static const struct reg_field ERROR_IRQ_CLR_fields[] = {
    FIELD_NUM(ERROR_IRQ_CLR, TBAT_ERROR_IRQ_CLR),
    FIELD_NUM(ERROR_IRQ_CLR, TDIE_ERROR_IRQ_CLR),
    FIELD_NUM(ERROR_IRQ_CLR, VBAT_OVP_ERROR_IRQ_CLR),
    FIELD_NUM(ERROR_IRQ_CLR, TOTAL_CHARGE_TIMEOUT_IRQ_CLR),
    FIELD_NUM(ERROR_IRQ_CLR, CV_CHARGE_TIMEOUT_IRQ_CLR),
    FIELD_NUM(ERROR_IRQ_CLR, CC_CHARGE_TIMEOUT_IRQ_CLR),
    FIELD_NUM(ERROR_IRQ_CLR, PRECHARGE_TIMEOUT_IRQ_CLR),
    { NULL }
};

#define REG(reg_name) { .addr = &CHARGER->CHARGER_##reg_name##_REG, .name = "CHARGER_"#reg_name"_REG", .fields = reg_name##_fields }

#else
#define da1469x_charger_cli_decode_fields false

#define REG(reg_name) { .addr = &CHARGER->CHARGER_##reg_name##_REG, .name = "CHARGER_"#reg_name"_REG", .fields = NULL }
#endif

#define REG_NAME(short_name) "CHARGER_"#short_name"_REG"

static const struct reg charger_regs[] = {
    REG(CTRL),
    REG(STATUS),
    REG(VOLTAGE_PARAM),
    REG(CURRENT_PARAM),
    REG(TEMPSET_PARAM),
    REG(PRE_CHARGE_TIMER),
    REG(CC_CHARGE_TIMER),
    REG(CV_CHARGE_TIMER),
    REG(TOTAL_CHARGE_TIMER),
    REG(JEITA_V_CHARGE),
    REG(JEITA_V_PRECHARGE),
    REG(JEITA_V_REPLENISH),
    REG(JEITA_V_OVP),
    REG(JEITA_CURRENT),
    REG(VBAT_COMP_TIMER),
    REG(VOVP_COMP_TIMER),
    REG(TDIE_COMP_TIMER),
    REG(TBAT_MON_TIMER),
    REG(TBAT_COMP_TIMER),
    REG(THOT_COMP_TIMER),
    REG(PWR_UP_TIMER),
    REG(STATE_IRQ_MASK),
    REG(ERROR_IRQ_MASK),
    REG(STATE_IRQ_STATUS),
    REG(ERROR_IRQ_STATUS),
    REG(STATE_IRQ_CLR),
    REG(ERROR_IRQ_CLR),
};
#define NUM_CHARGER_REGS ARRAY_SIZE(charger_regs)

static const struct shell_cmd da1469x_charger_shell_cmd_struct;

static int
da1469x_charger_help(void)
{
    console_printf("%s cmd\n", da1469x_charger_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\thelp\n");
#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)
    console_printf("\tdump [decode]\n");
#else
    console_printf("\tdump\n");
#endif
    console_printf("\tread <reg_name>\n");
    console_printf("\twrite <reg_anme> <value>\n");
    console_printf("\tdisable\n");
    console_printf("\tenable\n");
    console_printf("\tstatus\n");
    console_printf("\tseti <charge_i> [<precharge_i> [<eoc_percent>]]\n");
    console_printf("\tsetv <charge_v> [<precharge_v> [<replenish_v> [<ovp_v>]]]\n");
#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
    console_printf("\tlisten start | stop\n");
#endif
    return 0;
}

static int
da1469x_charger_shell_err_too_many_args(const char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);

    return SYS_EINVAL;
}

static int
da1469x_charger_shell_err_unknown_arg(const char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);

    return SYS_EINVAL;
}

#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)

static const char *
field_bin_value(const struct reg_field *field, uint32_t val, char *buf)
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
        buf[--bits] = (char)('0' + (val & 1));
        mask >>= 1;
        val >>= 1;
    }

    return buf;
}

static int
field_int_value(const struct reg_field *field, uint32_t val)
{
    return (int)((val & field->fld_mask) >> field->fld_pos);
}

static int
da1469x_charger_shell_cmd_decode(int argc, char **argv)
{
    long long val;
    int status;

    if (argc == 2) {
        val = parse_ll_bounds(argv[1], 0, 1, &status);
        if (status == 0) {
            da1469x_charger_cli_decode_fields = val != 0;
        }
    }
    console_printf("decode %d\n", (int)da1469x_charger_cli_decode_fields);

    return 0;
}

#endif /* MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE) */

static void
da1469x_charger_dump_register(const struct reg *reg, uint32_t val, bool decode)
{
#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)
    const struct reg_field *field;
    char binary[33];
    char buf[50];
#endif
    console_printf("%-30s = 0x%08x \n", reg->name, (int)val);
#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)
    if (decode && reg->fields) {
        for (field = reg->fields; field->fld_name; ++field) {
            if (field->fld_decode_value) {
                field->fld_decode_value(field, val, buf);
                if (field->fld_show_bits) {
                    console_printf("%32s = %s %s\n", field->fld_name,
                                   field_bin_value(field, val, binary), buf);
                } else {
                    console_printf("%32s = %s\n", field->fld_name, buf);
                }
            } else {
                if (field->fld_show_bits) {
                    console_printf("%32s = %s\n", field->fld_name,
                                   field_bin_value(field, val, binary));
                } else {
                    console_printf("%32s = %d\n", field->fld_name,
                                   field_int_value(field, val));
                }
            }
        }
    }
#endif
}

static int
da1469x_charger_shell_cmd_dump(int argc, char **argv)
{
    int i;
    uint32_t val;
    const struct reg *reg;
    bool decode = da1469x_charger_cli_decode_fields;

#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)
    if (argc > 2) {
        return da1469x_charger_shell_err_too_many_args(argv[1]);
    } else if (argc == 2) {
        if (strcmp(argv[1], "decode") == 0) {
            decode = true;
        } else if (strcmp(argv[1], "nodecode") == 0) {
            decode = false;
        } else {
            return da1469x_charger_shell_err_unknown_arg(argv[1]);
        }
    }
#else
    if (argc > 2) {
        return da1469x_charger_shell_err_too_many_args(argv[1]);
        return 0;
    }
#endif

    console_printf("========== Charger Regs ==========\n");
    console_printf("==================================\n\n");
    for (i = 0; i < NUM_CHARGER_REGS; ++i) {
        reg = &charger_regs[i];
        val = *reg->addr;
        da1469x_charger_dump_register(reg, val, decode);
    }

    return 0;
}

static int
da1469x_charger_shell_cmd_enable(int argc, char **argv)
{
    struct da1469x_charger_dev *dev =
        (struct da1469x_charger_dev *)os_dev_open("charger", 0, NULL);
    (void)argc;
    (void)argv;

    if (dev) {
        da1469x_charger_charge_enable(dev);
        os_dev_close(&dev->dev);
    }
    return 0;
}

static int
da1469x_charger_shell_cmd_disable(int argc, char **argv)
{
    struct da1469x_charger_dev *dev =
        (struct da1469x_charger_dev *)os_dev_open("charger", 0, NULL);
    (void)argc;
    (void)argv;

    if (dev) {
        da1469x_charger_charge_disable(dev);
        os_dev_close(&dev->dev);
    }
    return 0;
}

static int
da1469x_charger_shell_cmd_status(int argc, char **argv)
{
#if MYNEWT_VAL(SDADC_BATTERY) || MYNEWT_VAL(GPADC_BATTERY)
    struct adc_dev *adc;
    int bat_val;
#endif
    uint32_t val = CHARGER->CHARGER_STATUS_REG;
    uint32_t state = (val & CHARGER_CHARGER_STATUS_REG_CHARGER_STATE_Msk) >>
                      CHARGER_CHARGER_STATUS_REG_CHARGER_STATE_Pos;
    uint32_t bat_state = (val & CHARGER_CHARGER_STATUS_REG_TBAT_STATUS_Msk) >>
                          CHARGER_CHARGER_STATUS_REG_TBAT_STATUS_Pos;
    bool vbus_ok = (CRG_TOP->ANA_STATUS_REG & CRG_TOP_ANA_STATUS_REG_VBUS_AVAILABLE_Msk) != 0;
    const uint32_t mask = CHARGER_CHARGER_CTRL_REG_CHARGER_ENABLE_Msk |
                          CHARGER_CHARGER_CTRL_REG_CHARGE_START_Msk;

    (void)argc;
    (void)argv;
    if ((CHARGER->CHARGER_CTRL_REG & mask) != mask) {
        console_printf("status = disabled\n");
    } else if (!vbus_ok) {
        console_printf("status = enabled (not connected)\n");
    } else {
        console_printf("status = %s\n", charger_state[state]);
    }
    console_printf("  vbus = %s\n", vbus_ok ? "OK" : "NOK");
    console_printf("  tbat = %s\n", tbat_status[bat_state]);
#if MYNEWT_VAL(SDADC_BATTERY) || MYNEWT_VAL(GPADC_BATTERY)
    adc = (struct adc_dev *)da1469x_open_battery_adc(BATTERY_ADC_DEV_NAME, 1);
    if (adc) {
        adc_read_channel(adc, 0, &bat_val);
        os_dev_close(&adc->ad_dev);
        console_printf("  vbat = %d mV\n", adc_result_mv(adc, 0, bat_val));
    }
#endif

    return 0;
}

static int
da1469x_charger_shell_cmd_clear_irq(int argc, char **argv)
{
    if (argc == 1) {
        CHARGER->CHARGER_STATE_IRQ_CLR_REG = 0xFFFFFFFF;
        CHARGER->CHARGER_ERROR_IRQ_CLR_REG = 0xFFFFFFFF;
    } else if (0 == strcmp(argv[1], "state")) {
        CHARGER->CHARGER_STATE_IRQ_CLR_REG = 0xFFFFFFFF;
    } else if (0 == strcmp(argv[1], "error")) {
        CHARGER->CHARGER_ERROR_IRQ_CLR_REG = 0xFFFFFFFF;
    } else {
        return -1;
    }
    return 0;
}

static int
da1469x_charger_shell_cmd_set_i(int argc, char **argv)
{
    int rc;
    uint32_t val = CHARGER->CHARGER_CURRENT_PARAM_REG;
    uint32_t i_charge;
    uint32_t i_precharge;
    uint32_t i_eoc;

    if (argc > 1) {
        i_charge = (uint32_t)parse_ll_bounds(argv[1], 5, 560, &rc);
        if (rc) {
            console_printf("I_CHARGE should be in range 5-560\n");
            return 0;
        }
        val &= ~CHARGER_CHARGER_CURRENT_PARAM_REG_I_CHARGE_Msk;
        val |= DA1469X_ENCODE_CHG_I(i_charge);
    }
    if (argc > 2) {
        i_precharge = (uint32_t)parse_ll_bounds(argv[2], 1, 56, &rc);
        if (rc) {
            console_printf("I_PRECHARGE should be in range 1-56\n");
            return 0;
        }
        val &= ~CHARGER_CHARGER_CURRENT_PARAM_REG_I_PRECHARGE_Msk;
        val |= DA1469X_ENCODE_PRECHG_I(i_precharge);
    }
    if (argc > 3) {
        i_eoc = (uint32_t)parse_ll_bounds(argv[3], 4, 35, &rc);
        if (rc) {
            console_printf("I_EOC should be in range 4-35\n");
            return 0;
        }
        val &= ~(CHARGER_CHARGER_CURRENT_PARAM_REG_I_END_OF_CHARGE_Msk |
                 CHARGER_CHARGER_CURRENT_PARAM_REG_I_EOC_DOUBLE_RANGE_Msk);
        val |= DA1469X_ENCODE_EOC_I(i_eoc);
    }
    CHARGER->CHARGER_CURRENT_PARAM_REG = val;

    return 0;
}

static int
da1469x_charger_shell_cmd_set_v(int argc, char **argv)
{
    int rc;
    uint32_t val = CHARGER->CHARGER_VOLTAGE_PARAM_REG;
    uint32_t v;

    if (argc > 1 && strcmp("-", argv[1]) != 0) {
        v = (uint32_t)parse_ll_bounds(argv[1], 2800, 4900, &rc);
        if (rc) {
            console_printf("V_CHARGE should be in range 2800-4900\n");
            return 0;
        }
        val &= ~CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_CHARGE_Msk;
        val |= DA1469X_ENCODE_V(v) << CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_CHARGE_Pos;
    }
    if (argc > 2 && strcmp("-", argv[2]) != 0) {
        v = (uint32_t)parse_ll_bounds(argv[2], 2800, 4900, &rc);
        if (rc) {
            console_printf("V_PRECHARGE should be in range 2800-4900\n");
            return 0;
        }
        val &= ~CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_PRECHARGE_Msk;
        val |= DA1469X_ENCODE_V(v) << CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_PRECHARGE_Pos;
    }
    if (argc > 3 && strcmp("-", argv[3]) != 0) {
        v = (uint32_t)parse_ll_bounds(argv[3], 2800, 4900, &rc);
        if (rc) {
            console_printf("V_REPLENISH should be in range 2800-4900\n");
            return 0;
        }
        val &= ~CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_REPLENISH_Msk;
        val |= DA1469X_ENCODE_V(v) << CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_REPLENISH_Pos;
    }
    if (argc > 4 && strcmp("-", argv[4]) != 0) {
        v = (uint32_t)parse_ll_bounds(argv[4], 2800, 4900, &rc);
        if (rc) {
            console_printf("V_OVP should be in range 2800-4900\n");
            return 0;
        }
        val &= ~CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_OVP_Msk;
        val |= DA1469X_ENCODE_V(v) << CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_OVP_Pos;
    }
    CHARGER->CHARGER_VOLTAGE_PARAM_REG = val;

    return 0;
}

#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)

int
da1469x_charger_status_changed(struct charge_control *ctrl, void *arg,
                               void *val, charge_control_type_t type)
{
    charge_control_status_t status;
    charge_control_fault_t fault;

    if (type == CHARGE_CONTROL_TYPE_STATUS) {
        status = *(charge_control_status_t *)val;
        switch (status) {
        case CHARGE_CONTROL_STATUS_DISABLED:
            console_printf("charger: charging disabled\n");
            break;
        case CHARGE_CONTROL_STATUS_NO_SOURCE:
            console_printf("charger: not connected\n");
            break;
        case CHARGE_CONTROL_STATUS_CHARGING:
            console_printf("charger: charging\n");
            break;
        case CHARGE_CONTROL_STATUS_CHARGE_COMPLETE:
            console_printf("charger: charge complete\n");
            break;
        case CHARGE_CONTROL_STATUS_SUSPEND:
            console_printf("charger: charge suspended due to temperature\n");
            break;
        case CHARGE_CONTROL_STATUS_FAULT:
            console_printf("charger: fault Vbat to high or charge time exceeded\n");
            break;
        case CHARGE_CONTROL_STATUS_OTHER:
            break;
        }
    } else if (type == CHARGE_CONTROL_TYPE_FAULT) {
        fault = *(charge_control_fault_t *)val;
        switch (fault) {
        case CHARGE_CONTROL_FAULT_OV:
            console_printf("charger: fault overvoltage\n");
            break;
        case CHARGE_CONTROL_FAULT_THERM:
            console_printf("charger: fault over temperature\n");
            break;
        default:
            break;
        }
    }
    return 0;
}

struct charge_control_listener da1469x_listener = {
    .ccl_type = CHARGE_CONTROL_TYPE_STATUS | CHARGE_CONTROL_TYPE_FAULT,
    .ccl_func = da1469x_charger_status_changed,
};

static struct da1469x_charger_dev *charger;

static int
da1469x_charger_shell_cmd_listen(int argc, char **argv)
{
    if (argc > 1) {
        if (strcmp("start", argv[1]) == 0) {
            if (charger == NULL) {
                charger = (struct da1469x_charger_dev *)os_dev_open("charger", 1, NULL);
                if (charger) {
                    charge_control_register_listener(&charger->chg_ctrl, &da1469x_listener);
                }

            }
        } else if (strcmp("stop", argv[1]) == 0) {
            if (charger) {
                charge_control_unregister_listener(&charger->chg_ctrl, &da1469x_listener);
                os_dev_close(&charger->dev);
                charger = NULL;
            }
        }
    }
    return 0;
}

#endif /* MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL) */

static int
da1469x_charger_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return da1469x_charger_help();
    }

    argv++;
    argc--;
    if (strcmp(argv[0], "help") == 0) {
        return da1469x_charger_help();
#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)
    } else if (strcmp(argv[0], "decode") == 0) {
        return da1469x_charger_shell_cmd_decode(argc, argv);
#endif
    } else if (strcmp(argv[0], "dump") == 0) {
        return da1469x_charger_shell_cmd_dump(argc, argv);
    } else if (strcmp(argv[0], "enable") == 0) {
        return da1469x_charger_shell_cmd_enable(argc, argv);
    } else if (strcmp(argv[0], "disable") == 0) {
        return da1469x_charger_shell_cmd_disable(argc, argv);
    } else if (strcmp(argv[0], "status") == 0) {
        return da1469x_charger_shell_cmd_status(argc, argv);
    } else if (strcmp(argv[0], "seti") == 0) {
        return da1469x_charger_shell_cmd_set_i(argc, argv);
    } else if (strcmp(argv[0], "setv") == 0) {
        return da1469x_charger_shell_cmd_set_v(argc, argv);
#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
    } else if (strcmp(argv[0], "listen") == 0) {
        return da1469x_charger_shell_cmd_listen(argc, argv);
#endif
    }

    return da1469x_charger_shell_err_unknown_arg(argv[1]);
}

static const struct shell_cmd da1469x_charger_shell_cmd_struct = {
    .sc_cmd = "charger",
    .sc_cmd_func = da1469x_charger_shell_cmd
};

static int
da1469x_charger_reg_cmd(int argc, char **argv)
{
    const struct reg *reg = NULL;
    uint32_t val;
    int status;
    int i;
    bool dump = false;
    bool decode = da1469x_charger_cli_decode_fields;

    for (i = 0; i < NUM_CHARGER_REGS; ++i) {
        if (strcasecmp(charger_regs[i].name, argv[0]) == 0) {
            reg = charger_regs + i;
        }
    }
    if (!reg) {
        return 0;
    }

    if (argc == 1) {
        dump = true;
#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)
    } else if (strcmp(argv[1], "decode") == 0) {
        dump = true;
        decode = true;
#endif
    } else {
        val = (uint32_t)parse_ull_bounds(argv[1], 0, 0xFFFFFFFF, &status);
        if (status) {
            console_printf("Invalid register value %s\n", argv[1]);
            return 0;
        }

    }
    if (dump) {
        val = *reg->addr;
        da1469x_charger_dump_register(reg, val, decode);
    } else {
        *reg->addr = val;
    }

    return 0;
}

static const struct shell_cmd da1469x_charger_cmds[] = {
#if MYNEWT_VAL(DA1469X_CHARGER_CLI_DECODE)
    SHELL_CMD("decode", da1469x_charger_shell_cmd_decode, HELP(decode)),
#endif
    SHELL_CMD("dump", da1469x_charger_shell_cmd_dump, HELP(dump)),
    SHELL_CMD("enable", da1469x_charger_shell_cmd_enable, HELP(enable)),
    SHELL_CMD("disable", da1469x_charger_shell_cmd_disable, HELP(disable)),
    SHELL_CMD("status", da1469x_charger_shell_cmd_status, HELP(status)),
    SHELL_CMD("clrirq", da1469x_charger_shell_cmd_clear_irq, HELP(clear_irq)),
    SHELL_CMD("seti", da1469x_charger_shell_cmd_set_i, HELP(set_i)),
    SHELL_CMD("setv", da1469x_charger_shell_cmd_set_v, HELP(set_v)),
#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
    SHELL_CMD("listen", da1469x_charger_shell_cmd_listen, HELP(listen)),
#endif
    SHELL_CMD(REG_NAME(CTRL), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(STATUS), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(VOLTAGE_PARAM), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(CURRENT_PARAM), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(TEMPSET_PARAM), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(PRE_CHARGE_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(CC_CHARGE_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(CV_CHARGE_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(TOTAL_CHARGE_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(JEITA_V_CHARGE), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(JEITA_V_PRECHARGE), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(JEITA_V_REPLENISH), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(JEITA_V_OVP), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(JEITA_CURRENT), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(VBAT_COMP_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(VOVP_COMP_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(TDIE_COMP_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(TBAT_MON_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(TBAT_COMP_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(THOT_COMP_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(PWR_UP_TIMER), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(STATE_IRQ_MASK), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(ERROR_IRQ_MASK), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(STATE_IRQ_STATUS), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(ERROR_IRQ_STATUS), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(STATE_IRQ_CLR), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(REG_NAME(ERROR_IRQ_CLR), da1469x_charger_reg_cmd, HELP(reg)),
    SHELL_CMD(NULL, NULL, NULL)
};

int
da1469x_charger_shell_init(struct da1469x_charger_dev *dev)
{
    int rc;
    (void)dev;

    rc = shell_cmd_register(&da1469x_charger_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = shell_register("charger", da1469x_charger_cmds);
    return rc;
}

#endif
