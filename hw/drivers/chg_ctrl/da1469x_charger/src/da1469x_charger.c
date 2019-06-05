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
#include <assert.h>

#include <os/mynewt.h>
#include <da1469x_charger/da1469x_charger.h>
#include <bsp/bsp.h>
#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
#include <charge-control/charge_control.h>
#include <DA1469xAB.h>
#endif

int
da1469x_charger_set_charge_currents(struct da1469x_charger_dev *dev,
                            uint16_t ichg, uint16_t iprechg, uint8_t ieoc)
{
    uint16_t current_param;

    if (dev == NULL || ichg < 5 || ichg > 560 || iprechg < 1 || iprechg > 56 ||
        ieoc < 6 || ieoc > 40) {
        return SYS_EINVAL;
    }

    /* Charge current */
    current_param = DA1469X_ENCODE_CHG_I(ichg);

    /* Pre-charge current */
    current_param |= DA1469X_ENCODE_PRECHG_I(iprechg);

    /* End of charge current as % of charge current */
    current_param |= DA1469X_ENCODE_EOC_I(ieoc);

    CHARGER->CHARGER_CURRENT_PARAM_REG = current_param;

    return 0;
}

static inline bool
bad_v(uint16_t v)
{
    return v < 2800 || v > 4900;
}

static uint16_t
encode_v(uint16_t v)
{
    return (uint16_t)DA1469X_ENCODE_V(v);
}

int
da1469x_charger_set_charge_voltages(struct da1469x_charger_dev *dev,
                                    uint16_t vchrg, uint16_t vprechg,
                                    uint16_t vreplenish, uint16_t ov)
{
    uint16_t voltage_param;

    if (dev == NULL || bad_v(vchrg) || bad_v(vprechg) || bad_v(vreplenish) ||
        bad_v(ov)) {
        return SYS_EINVAL;
    }

    voltage_param = encode_v(vchrg) |
        (encode_v(vprechg) << CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_PRECHARGE_Pos) |
        (encode_v(vreplenish) << CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_REPLENISH_Pos) |
        (encode_v(ov) << CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_OVP_Pos);

    CHARGER->CHARGER_VOLTAGE_PARAM_REG = voltage_param;

    return 0;
}

#define SET_IF_VALID(reg_name, field_name) \
    if (config->field_name##_valid) { \
        CHARGER->CHARGER_##reg_name##_REG = config->field_name; \
    }

int
da1469x_charger_set_config(struct da1469x_charger_dev *dev,
                           const struct da1469x_charger_config *config)
{
    (void)dev;

    SET_IF_VALID(CTRL, ctrl);
    SET_IF_VALID(VOLTAGE_PARAM, voltage_param);
    SET_IF_VALID(CURRENT_PARAM, current_param);
    SET_IF_VALID(TEMPSET_PARAM, tempset_param);
    SET_IF_VALID(PRE_CHARGE_TIMER, pre_charge_timer);
    SET_IF_VALID(CC_CHARGE_TIMER, cc_charge_timer);
    SET_IF_VALID(CV_CHARGE_TIMER, cv_charge_timer);
    SET_IF_VALID(JEITA_V_CHARGE, jeita_v_charge);
    SET_IF_VALID(JEITA_V_PRECHARGE, jeita_v_precharge);
    SET_IF_VALID(JEITA_V_REPLENISH, jeita_v_replenish);
    SET_IF_VALID(JEITA_V_OVP, jeita_v_ovp);
    SET_IF_VALID(JEITA_CURRENT, jeita_current);
    SET_IF_VALID(VBAT_COMP_TIMER, vbat_comp_timer);
    SET_IF_VALID(VOVP_COMP_TIMER, vovp_comp_timer);
    SET_IF_VALID(TDIE_COMP_TIMER, tdie_comp_timer);
    SET_IF_VALID(TBAT_MON_TIMER, tbat_mon_timer);
    SET_IF_VALID(TBAT_COMP_TIMER, tbat_comp_timer);
    SET_IF_VALID(THOT_COMP_TIMER, thot_comp_timer);
    SET_IF_VALID(PWR_UP_TIMER, pwr_up_timer);

    return 0;
}

#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
static void
da1469x_vbus_state_changed_event_cb(struct os_event *ev)
{
    assert(ev);
    NVIC_SetPendingIRQ(CHARGER_STATE_IRQn);
}

static struct os_event da1469x_vbus_state_changed_event = {
    .ev_cb = da1469x_vbus_state_changed_event_cb,
};

static void
da1469x_vbus_isr(void)
{
    CRG_TOP->VBUS_IRQ_CLEAR_REG = 1;
    os_eventq_put(os_eventq_dflt_get(), &da1469x_vbus_state_changed_event);
}

void da1469x_usb_init(void)
{
    NVIC_SetVector(VBUS_IRQn, (uint32_t)da1469x_vbus_isr);
    CRG_TOP->VBUS_IRQ_CLEAR_REG = 1;
    NVIC_ClearPendingIRQ(VBUS_IRQn);
    CRG_TOP->VBUS_IRQ_MASK_REG = CRG_TOP_VBUS_IRQ_MASK_REG_VBUS_IRQ_EN_FALL_Msk |
                                 CRG_TOP_VBUS_IRQ_MASK_REG_VBUS_IRQ_EN_RISE_Msk;
    NVIC_EnableIRQ(VBUS_IRQn);
}

static void
da1469x_charger_state_changed_cb(struct os_event *ev)
{
    struct da1469x_charger_dev *dev;
    uint32_t irq_state;

    assert(ev);
    dev = ev->ev_arg;

    irq_state = CHARGER->CHARGER_STATE_IRQ_STATUS_REG;
    CHARGER->CHARGER_STATE_IRQ_CLR_REG = irq_state;
    /* Interrupt generated by charger triggers out of schedule read */
    charge_control_read(&dev->chg_ctrl, CHARGE_CONTROL_TYPE_STATUS,
                        NULL, NULL, OS_TIMEOUT_NEVER);
}

static struct os_event da1469x_charger_state_changed_event = {
    .ev_cb = da1469x_charger_state_changed_cb,
};

static void
da1469x_charger_state_isr(void)
{
    os_eventq_put(os_eventq_dflt_get(), &da1469x_charger_state_changed_event);
}

static void
da1469x_charger_error_cb(struct os_event *ev)
{
    struct da1469x_charger_dev *dev;
    uint32_t irq_state;

    assert(ev);
    dev = ev->ev_arg;

    irq_state = CHARGER->CHARGER_ERROR_IRQ_STATUS_REG;
    CHARGER->CHARGER_ERROR_IRQ_CLR_REG = irq_state;

    /* Interrupt generated by charger triggers out of schedule read */
    charge_control_read(&dev->chg_ctrl, CHARGE_CONTROL_TYPE_FAULT,
                        NULL, NULL, OS_TIMEOUT_NEVER);
}

static struct os_event da1469x_charger_error_event = {
    .ev_cb = da1469x_charger_error_cb,
};

static void
da1469x_charger_error_isr(void)
{
    os_eventq_put(os_eventq_dflt_get(), &da1469x_charger_error_event);
}
#endif

void
da1469x_charger_clock_enable(struct da1469x_charger_dev *dev)
{
    (void)dev;

    CRG_SYS->CLK_SYS_REG |= CRG_SYS_CLK_SYS_REG_CLK_CHG_EN_Msk;
}

void
da1469x_charger_clock_disable(struct da1469x_charger_dev *dev)
{
    (void)dev;

    CRG_SYS->CLK_SYS_REG &= ~CRG_SYS_CLK_SYS_REG_CLK_CHG_EN_Msk;
}

int
da1469x_charger_charge_enable(struct da1469x_charger_dev *dev)
{
    const uint32_t mask = CHARGER_CHARGER_CTRL_REG_CHARGER_ENABLE_Msk |
                          CHARGER_CHARGER_CTRL_REG_CHARGE_START_Msk;
    da1469x_charger_clock_enable(dev);
    if ((CHARGER->CHARGER_CTRL_REG & mask) != mask) {
        CHARGER->CHARGER_CTRL_REG |= mask;
    }

    return 0;
}

int
da1469x_charger_charge_disable(struct da1469x_charger_dev *dev)
{
    const uint32_t mask = CHARGER_CHARGER_CTRL_REG_CHARGER_ENABLE_Msk |
                          CHARGER_CHARGER_CTRL_REG_CHARGE_START_Msk;
    (void)dev;

    if ((CHARGER->CHARGER_CTRL_REG & mask) != 0) {
        CHARGER->CHARGER_CTRL_REG &= ~mask;
        NVIC_SetPendingIRQ(CHARGER_STATE_IRQn);
    }

    return 0;
}

int
da1469x_charger_set_state_change_irq_mask(struct da1469x_charger_dev *dev,
                                          da1469x_charger_state_irq_t mask)
{
    (void)dev;

    CHARGER->CHARGER_STATE_IRQ_MASK_REG = mask;

    return 0;
}

int
da1469x_charger_set_error_irq_mask(struct da1469x_charger_dev *dev,
                                   uint16_t mask)
{
    (void)dev;

    CHARGER->CHARGER_ERROR_IRQ_MASK_REG = mask;

    return 0;
}

#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
static int
da1469x_charger_chg_ctrl_read(struct charge_control *cc, charge_control_type_t type,
        charge_control_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    const uint32_t mask = CHARGER_CHARGER_CTRL_REG_CHARGER_ENABLE_Msk |
                          CHARGER_CHARGER_CTRL_REG_CHARGE_START_Msk;
    charge_control_status_t status = CHARGE_CONTROL_STATUS_DISABLED;
    charge_control_fault_t fault = CHARGE_CONTROL_FAULT_NONE;
    struct da1469x_charger_dev *dev = (struct da1469x_charger_dev *)cc->cc_dev;
    (void)timeout;

    if (type & CHARGE_CONTROL_TYPE_STATUS) {
        if ((CHARGER->CHARGER_CTRL_REG & mask) != mask) {
            status = CHARGE_CONTROL_STATUS_DISABLED;
        } else if (!(CRG_TOP->ANA_STATUS_REG &
              CRG_TOP_ANA_STATUS_REG_VBUS_AVAILABLE_Msk)) {
            status = CHARGE_CONTROL_STATUS_NO_SOURCE;
        } else {
            switch (da1469x_charger_get_state(dev)) {
            case DA1469X_CHARGER_STATE_POWER_UP:
            case DA1469X_CHARGER_STATE_INIT:
            case DA1469X_CHARGER_STATE_DISABLED:
                status = CHARGE_CONTROL_STATUS_DISABLED;
                break;
            case DA1469X_CHARGER_STATE_CC_CHARGE:
            case DA1469X_CHARGER_STATE_CV_CHARGE:
            case DA1469X_CHARGER_STATE_PRE_CHARGE:
                status = CHARGE_CONTROL_STATUS_CHARGING;
                break;
            case DA1469X_CHARGER_STATE_END_OF_CHARGE:
                status = CHARGE_CONTROL_STATUS_CHARGE_COMPLETE;
                break;
            case DA1469X_CHARGER_STATE_TBAT_PROT:
            case DA1469X_CHARGER_STATE_TDIE_PROT:
                status = CHARGE_CONTROL_STATUS_SUSPEND;
                break;
            case DA1469X_CHARGER_STATE_ERROR:
                status = CHARGE_CONTROL_STATUS_FAULT;
                break;
            default:
                break;
            }
        }
        if (data_func) {
            data_func(cc, data_arg, &status, CHARGE_CONTROL_TYPE_STATUS);
        }
    }
    if (type & CHARGE_CONTROL_TYPE_FAULT) {
        if (CHARGER->CHARGER_STATUS_REG &
            CHARGER_CHARGER_STATUS_REG_OVP_EVENTS_DEBOUNCE_CNT_Msk) {
            fault |= CHARGE_CONTROL_FAULT_OV;
        }
        if (CHARGER->CHARGER_STATUS_REG &
            (CHARGER_CHARGER_STATUS_REG_TBAT_HOT_COMP_OUT_Msk |
             CHARGER_CHARGER_STATUS_REG_TDIE_COMP_OUT_Msk)) {
            fault |= CHARGE_CONTROL_FAULT_THERM;
        }
        if (fault && data_func) {
            data_func(cc, data_arg, &fault, CHARGE_CONTROL_TYPE_FAULT);
        }
    }

    return 0;
}

static int
da1469x_charger_chg_ctrl_get_config(struct charge_control *cc,
                                    charge_control_type_t type,
                                    struct charge_control_cfg *cfg)
{
    (void)cc;
    (void)type;
    (void)cfg;

    return SYS_ENOTSUP;
}

static int
da1469x_charger_chg_ctrl_set_config(struct charge_control *cc, void *cfg)
{
    (void)cc;
    (void)cfg;

    return SYS_ENOTSUP;
}

static int
da1469x_charger_chg_ctrl_get_status(struct charge_control *cc, int *status)
{
    struct da1469x_charger_dev *dev = (struct da1469x_charger_dev *)cc->cc_dev;

    *status = (int)da1469x_charger_get_state(dev);

    return 0;
}

static int
da1469x_charger_chg_ctrl_get_fault(struct charge_control *cc,
                                   charge_control_fault_t *fault)
{
    *fault = CHARGE_CONTROL_FAULT_NONE;

    switch (da1469x_charger_get_state((struct da1469x_charger_dev *)cc->cc_dev)) {
    case DA1469X_CHARGER_STATE_TDIE_PROT:
    case DA1469X_CHARGER_STATE_TBAT_PROT:
        *fault |= CHARGE_CONTROL_FAULT_THERM;
        break;
    default:
        break;
    }

    if (CHARGER->CHARGER_STATUS_REG &
        CHARGER_CHARGER_STATUS_REG_VBAT_OVP_COMP_OUT_Msk) {
        *fault |= CHARGE_CONTROL_FAULT_OV;
    }

    return 0;
}

static int
da1469x_charger_chg_ctrl_enable(struct charge_control *cc)
{
    return da1469x_charger_charge_enable((struct da1469x_charger_dev *)cc->cc_dev);
}

static int
da1469x_charger_chg_ctrl_disable(struct charge_control *cc)
{
    return da1469x_charger_charge_disable((struct da1469x_charger_dev *)cc->cc_dev);
}

/* Exports for the charge control API */
static const struct charge_control_driver da1469x_charger_chg_ctrl_driver = {
    .ccd_read = da1469x_charger_chg_ctrl_read,
    .ccd_get_config = da1469x_charger_chg_ctrl_get_config,
    .ccd_set_config = da1469x_charger_chg_ctrl_set_config,
    .ccd_get_status = da1469x_charger_chg_ctrl_get_status,
    .ccd_get_fault = da1469x_charger_chg_ctrl_get_fault,
    .ccd_enable = da1469x_charger_chg_ctrl_enable,
    .ccd_disable = da1469x_charger_chg_ctrl_disable,
};
#endif /* DA1469X_CHARGER_USE_CHARGE_CONTROL */

int
da1469x_charger_init(struct os_dev *dev, void *arg)
{
    struct da1469x_charger_dev *charger = (struct da1469x_charger_dev *)dev;
#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
    struct charge_control *cc;
#endif
    const struct da1469x_charger_config *cfg = arg;
    int rc = 0;

    if (!dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    if (cfg) {
        rc = da1469x_charger_set_config(charger, cfg);
        if (rc) {
            goto err;
        }
    }

#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
    cc = &charger->chg_ctrl;

    rc = charge_control_init(cc, dev);
    if (rc) {
        goto err;
    }

    /* Add the driver with all the supported types */
    rc = charge_control_set_driver(cc, CHARGE_CONTROL_TYPE_STATUS |
        CHARGE_CONTROL_TYPE_FAULT,
        (struct charge_control_driver *)&da1469x_charger_chg_ctrl_driver);
    if (rc) {
        goto err;
    }

    charge_control_set_type_mask(cc,
                                 CHARGE_CONTROL_TYPE_STATUS |
                                 CHARGE_CONTROL_TYPE_FAULT);

    rc = charge_control_mgr_register(cc);
    if (rc) {
        goto err;
    }

    da1469x_charger_state_changed_event.ev_arg = charger;
    da1469x_charger_error_event.ev_arg = charger;

    da1469x_charger_set_error_irq_mask(charger, DA1469X_CHARGER_ERROR_IRQ_ALL);
    da1469x_charger_set_state_change_irq_mask(charger,
                                              DA1469X_CHARGER_STATE_IRQ_DISABLED_TO_PRECHARGE |
                                              DA1469X_CHARGER_STATE_IRQ_CC_TO_EOC |
                                              DA1469X_CHARGER_STATE_IRQ_CV_TO_EOC |
                                              DA1469X_CHARGER_STATE_IRQ_EOC_TO_PRECHARGE |
                                              DA1469X_CHARGER_STATE_IRQ_TDIE_PROT_TO_PRECHARGE |
                                              DA1469X_CHARGER_STATE_IRQ_TBAT_PROT_TO_PRECHARGE |
                                              DA1469X_CHARGER_STATE_IRQ_TBAT_STATUS_UPDATE);

    NVIC_SetVector(CHARGER_STATE_IRQn, (uint32_t)da1469x_charger_state_isr);
    NVIC_ClearPendingIRQ(CHARGER_STATE_IRQn);
    NVIC_EnableIRQ(CHARGER_STATE_IRQn);

    NVIC_SetVector(CHARGER_ERROR_IRQn, (uint32_t)da1469x_charger_error_isr);
    NVIC_ClearPendingIRQ(CHARGER_ERROR_IRQn);
    NVIC_EnableIRQ(CHARGER_ERROR_IRQn);

    da1469x_usb_init();
#endif /* DA1469X_CHARGER_USE_CHARGE_CONTROL */

#if MYNEWT_VAL(DA1469X_CHARGER_CLI)
    da1469x_charger_shell_init(charger);
#endif

err:
    return rc;
}

int
da1469x_charger_create(struct da1469x_charger_dev *dev, const char *name,
    struct da1469x_charger_config *cfg)
{
    int rc;

    rc = os_dev_create(&dev->dev, name,
                       OS_DEV_INIT_PRIMARY, OS_DEV_INIT_PRIO_DEFAULT,
                       da1469x_charger_init, cfg);

    return rc;
}
