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

#ifndef _DA1469X_CHARGER_H
#define _DA1469X_CHARGER_H

#include <os/os.h>
#include "os/os_dev.h"
#include "syscfg/syscfg.h"
#include "os/os_time.h"
#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
#include "charge-control/charge_control.h"
#endif

struct da1469x_charger_config {
    uint32_t ctrl_valid:1;
    uint32_t voltage_param_valid:1;
    uint32_t current_param_valid:1;
    uint32_t tempset_param_valid:1;
    uint32_t pre_charge_timer_valid:1;
    uint32_t cc_charge_timer_valid:1;
    uint32_t cv_charge_timer_valid:1;
    uint32_t total_charge_timer_valid:1;
    uint32_t jeita_v_charge_valid:1;
    uint32_t jeita_v_precharge_valid:1;
    uint32_t jeita_v_replenish_valid:1;
    uint32_t jeita_v_ovp_valid:1;
    uint32_t jeita_current_valid:1;
    uint32_t vbat_comp_timer_valid:1;
    uint32_t vovp_comp_timer_valid:1;
    uint32_t tdie_comp_timer_valid:1;
    uint32_t tbat_mon_timer_valid:1;
    uint32_t tbat_comp_timer_valid:1;
    uint32_t thot_comp_timer_valid:1;
    uint32_t pwr_up_timer_valid:1;

    uint32_t ctrl;
    uint32_t voltage_param;
    uint32_t current_param;
    uint32_t tempset_param;
    uint16_t pre_charge_timer;
    uint16_t cc_charge_timer;
    uint16_t cv_charge_timer;
    uint16_t total_charge_timer;
    uint16_t jeita_v_charge;
    uint16_t jeita_v_precharge;
    uint16_t jeita_v_replenish;
    uint16_t jeita_v_ovp;
    uint32_t jeita_current;
    uint16_t vbat_comp_timer;
    uint16_t vovp_comp_timer;
    uint16_t tdie_comp_timer;
    uint16_t tbat_mon_timer;
    uint16_t tbat_comp_timer;
    uint16_t thot_comp_timer;
    uint16_t pwr_up_timer;
};

struct da1469x_charger_dev {
    struct os_dev dev;
#if MYNEWT_VAL(DA1469X_CHARGER_USE_CHARGE_CONTROL)
    struct charge_control   chg_ctrl;
#endif
};

#define DA1469X_ENCODE_V(v) \
    (((v) < 3800) ? (((v) - 2800) / 50) : \
                    (((v) < 4600) ? ((v) - 3800) / 20 + 20 : \
                                    ((v) - 4600) / 100 + 60))

#define DA1469X_ENCODE_CHG_I(i) \
    (uint16_t)(((i) < 85) ? ((i) / 5) - 1 : \
                  ((i) < 250 ? ((i) / 10) - 8 + 15 : \
                               ((i) / 20) - 12 + 31))

#define DA1469X_ENCODE_PRECHG_I(i) \
    ((uint16_t)((i) < 9 ? (i) * 2 - 1 : \
                (i) < 25 ? (i) - 8 + 15 : \
                           (i) / 2 - 12 + 31) << \
        CHARGER_CHARGER_CURRENT_PARAM_REG_I_PRECHARGE_Pos)

#define DA1469X_ENCODE_EOC_I(i) \
    ((uint16_t)((i) <= 10 ? ((i) - 4) * 2 / 3 : \
                    (i) < 18 ? 4 + ((i) - 10) / 2 : \
                    ((((i) - 12) / 4) + 8)) << \
        CHARGER_CHARGER_CURRENT_PARAM_REG_I_END_OF_CHARGE_Pos)

typedef enum {
    DA1469X_CHARGER_STATE_POWER_UP,
    DA1469X_CHARGER_STATE_INIT,
    DA1469X_CHARGER_STATE_DISABLED,
    DA1469X_CHARGER_STATE_PRE_CHARGE,
    DA1469X_CHARGER_STATE_CC_CHARGE,
    DA1469X_CHARGER_STATE_CV_CHARGE,
    DA1469X_CHARGER_STATE_END_OF_CHARGE,
    DA1469X_CHARGER_STATE_TDIE_PROT,
    DA1469X_CHARGER_STATE_TBAT_PROT,
    DA1469X_CHARGER_STATE_BYPASS,
    DA1469X_CHARGER_STATE_ERROR,
} da1469x_charger_state_t;

typedef enum {
    DA1469X_CHARGER_STATE_IRQ_NONE                      = 0x0000U,
    DA1469X_CHARGER_STATE_IRQ_DISABLED_TO_PRECHARGE     = 0x0001U,
    DA1469X_CHARGER_STATE_IRQ_PRECHARGE_TO_CC           = 0x0002U,
    DA1469X_CHARGER_STATE_IRQ_CC_TO_CV                  = 0x0004U,
    DA1469X_CHARGER_STATE_IRQ_CC_TO_EOC                 = 0x0008U,
    DA1469X_CHARGER_STATE_IRQ_CV_TO_EOC                 = 0x0010U,
    DA1469X_CHARGER_STATE_IRQ_EOC_TO_PRECHARGE          = 0x0020U,
    DA1469X_CHARGER_STATE_IRQ_TDIE_PROT_TO_PRECHARGE    = 0x0040U,
    DA1469X_CHARGER_STATE_IRQ_TBAT_PROT_TO_PRECHARGE    = 0x0080U,
    DA1469X_CHARGER_STATE_IRQ_TBAT_STATUS_UPDATE        = 0x0100U,
    DA1469X_CHARGER_STATE_IRQ_CV_TO_CC                  = 0x0200U,
    DA1469X_CHARGER_STATE_IRQ_CC_TO_PRECHARGE           = 0x0400U,
    DA1469X_CHARGER_STATE_IRQ_CV_TO_PRECHARGE           = 0x0800U,
    DA1469X_CHARGER_STATE_IRQ_ALL                       = 0x0FFFU,
} da1469x_charger_state_irq_t;

typedef enum {
    DA1469X_CHARGER_ERROR_IRQ_NONE                      = 0x00U,
    DA1469X_CHARGER_ERROR_IRQ_PRECHARGE_TIMEOUT         = 0x01U,
    DA1469X_CHARGER_ERROR_IRQ_CC_CHARGE_TIMEOUT         = 0x02U,
    DA1469X_CHARGER_ERROR_IRQ_CV_CHARGE_TIMEOUT         = 0x04U,
    DA1469X_CHARGER_ERROR_IRQ_TOTAL_CHARGE_TIMEOUT      = 0x08U,
    DA1469X_CHARGER_ERROR_IRQ_VBAT_OVP_ERROR            = 0x10U,
    DA1469X_CHARGER_ERROR_IRQ_TDIE_ERROR                = 0x20U,
    DA1469X_CHARGER_ERROR_IRQ_TBAT_ERROR                = 0x40U,
    DA1469X_CHARGER_ERROR_IRQ_ALL                       = 0x7FU,
} da1469x_charger_error_irq_t;

/**
* Set charge currents
*
* @param dev the charger device
* @param ichg charge current (5-560) mA
* @param iprechg pre-charge current (1-56) mA
* @param ieoc end-of-charge (percent of charge current 4-40)
*
* @return 0 - on success, SYS_EINVAL for invalid parameters
*/
int da1469x_charger_set_charge_currents(struct da1469x_charger_dev *dev,
                                        uint16_t ichg, uint16_t iprechg,
                                        uint8_t ieoc);

/**
* Set charge voltages
*
* @note: All voltage levels must be in 2800-4900 mV range
*
* @param dev the charger device
* @param vchrg charge voltage threshold
* @param vprechg pre-charge voltage threshold
* @param vreplenish recharge voltage threshold
* @param ov over voltage protection limit
*
* @return  0 - on success, SYS_EINVAL for invalid parameters
*/
int da1469x_charger_set_charge_voltages(struct da1469x_charger_dev *dev,
                                        uint16_t vchrg, uint16_t vprechg,
                                        uint16_t vreplenish, uint16_t ov);


/**
* Set charger configuration
*
* @param dev charger device
* @param config new configuration to set
*
* @return  0 - on success, SYS_EINVAL for invalid parameters
*/
int da1469x_charger_set_config(struct da1469x_charger_dev *dev,
                               const struct da1469x_charger_config *config);

/**
 * Set state change interrupt mask
 *
 * Interrupt will be trigger on each state change transition.
 *
 * @param dev charger device
 * @param mask new bit mask of enabled state change interrupts
 *             value should be a bitmask from da1469x_charger_state_irq_t
 *
 * @return  0 - on success, SYS_EINVAL for invalid parameters
 */
int da1469x_charger_set_state_change_irq_mask(struct da1469x_charger_dev *dev,
                                              uint16_t mask);

/**
 * Set error interrupt mask
 *
 * @param dev charger device
 * @param mask new bit mask of enabled error interrupts
 *             value should be a bitmask from da1469x_charger_error_irq_t
 *
 * @return  0 - on success, SYS_EINVAL for invalid parameters
 */
int da1469x_charger_set_error_irq_mask(struct da1469x_charger_dev *dev,
                                       uint16_t mask);

static inline da1469x_charger_state_t
da1469x_charger_get_state(struct da1469x_charger_dev *dev)
{
    return (da1469x_charger_state_t)((CHARGER->CHARGER_STATUS_REG &
         CHARGER_CHARGER_STATUS_REG_CHARGER_STATE_Msk) >>
                             CHARGER_CHARGER_STATUS_REG_CHARGER_STATE_Pos);
}

/**
 * Function called from os_dev_create
 *
 * @note: In normal case use wrapper function da1469x_charger_create()
 *
 * @param dev ad1469x_charger_dev to initialize
 * @param arg da1469x_charger_config initial configuration
 *
 * @return  0 - on success, SYS_EINVAL for invalid parameters
 */
int da1469x_charger_init(struct os_dev *dev, void *arg);

/**
 *
 * @param dev ad1469x_charger_dev to initialize
 * @param name device name to create
 * @param cfg da1469x_charger_config initial configuration
 * @return
 */
int da1469x_charger_create(struct da1469x_charger_dev *dev, const char *name,
                           struct da1469x_charger_config *cfg);


/**
 * Function enable charging.
 *
 * @param dev ad1469x_charger_dev
 *
 * @return  0 - on success, non-zero on failure
 */
int da1469x_charger_charge_enable(struct da1469x_charger_dev *dev);

/**
 * Function disables charging.
 *
 * @param dev ad1469x_charger_dev
 *
 * @return  0 - on success, non-zero on failure
 */
int da1469x_charger_charge_disable(struct da1469x_charger_dev *dev);

#if MYNEWT_VAL(DA1469X_CHARGER_CLI)
int da1469x_charger_shell_init(struct da1469x_charger_dev *dev);
#endif

#endif /* _DA1469X_CHARGER_H */
