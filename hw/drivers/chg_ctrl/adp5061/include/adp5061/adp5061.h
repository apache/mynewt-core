/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
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

#ifndef _ADP5061_H
#define _ADP5061_H

#include <os/os.h>
#include "os/os_dev.h"
#include "syscfg/syscfg.h"
#include "os/os_time.h"
#include "charge-control/charge_control.h"

/**
* Struct for ADP50961 configuration
*/
struct adp5061_config {
    uint8_t vinx_pin_settings;
    uint8_t termination_settings;
    uint8_t charging_current;
    uint8_t voltage_thresholds;
    uint8_t timer_settings;
    uint8_t functional_settings_1;
    uint8_t functional_settings_2;
    uint8_t interrupt_enable;
    uint8_t battery_short;
    uint8_t iend;
};

struct adp5061_dev {
    struct os_dev           a_dev;
    struct charge_control   a_chg_ctrl;
    struct adp5061_config   a_cfg;
    os_time_t               a_last_read_time;
};

/**
*ADP5061 Register mapping
*/
typedef enum {
    REG_PART_ID             = 0x00,
    REG_SILICON_REV         = 0x01,
    REG_VIN_PIN_SETTINGS    = 0x02,
    REG_TERM_SETTINGS       = 0x03,
    REG_CHARGING_CURRENT    = 0x04,
    REG_VOLTAGE_THRES       = 0x05,
    REG_TIMER_SETTINGS      = 0x06,
    REG_FUNC_SETTINGS_1     = 0x07,
    REG_FUNC_SETTINGS_2     = 0x08,
    REG_INT_EN              = 0x09,
    REG_INT_ACTIVE          = 0x0A,
    REG_CHARGER_STATUS_1    = 0x0B,
    REG_CHARGER_STATUS_2    = 0x0C,
    REG_FAULT_REGISTER      = 0x0D,
    REG_BATT_SHORT          = 0x10,
    REG_IEND                = 0x11,
} adp5061_device_regs_e;


/**
* Init function for ADP5061 charger
* @return error
*/
int adp5061_init(struct os_dev *dev, void *arg);

/**
* Get charger device id
*
* @param the charger device
* @param pointer to received data
*
* @return -1 if I2C-failure, 0 if OK
*/
int adp5061_get_device_id(struct adp5061_dev *dev, uint8_t *dev_id);

/**
* Set charge currents
*
* @param the charger device
* @param fast charge current
* @param trickle charge current
* @param input limit current
*
* @return -1 if I2C-failure, 0 if OK
*/
int adp5061_set_charge_currents(struct adp5061_dev *dev,
        uint8_t ichg, uint8_t itrk_dead, uint8_t i_lim);

/**
* Set charge voltages
*
* @param the charger device
* @param weak charge voltage threshold
* @param trickle charge voltage threshold
* @param termination voltage threshold
* @param charging voltage limit
* @param recharge voltage
*
* @return -1 if I2C-failure, 0 if OK
*/
int adp5061_set_charge_voltages(struct adp5061_dev *dev,
        uint8_t vweak, uint8_t vtrk_dead, uint8_t vtrm, uint8_t chg_vlim,
        uint8_t vrch);

/**
* Enable interrupts
*
* @param the charger device
* @param interrupt mask
*
* @return -1 if I2C-failure, 0 if OK
*/
int adp5061_enable_int(struct adp5061_dev *dev, uint8_t mask);

/**
* Enable recharge
*
* @param the charger device
*
* @return -1 if I2C-failure, 0 if OK
*/
int adp5061_recharge_enable(struct adp5061_dev *dev);

/**
* Disable recharge
*
* @param the charger device
*
* @return  -1 if I2C-failure, 0 if OK
*/
int adp5061_recharge_disable(struct adp5061_dev *dev);

/**
* Enable charging
*
* @param the charger device
* @param interrupt mask
*
* @return -1 if I2C-failure, 0 if OK
*/
int adp5061_charge_enable(struct adp5061_dev *dev);

/**
* Disable charging
*
* @param the charger device
*
* @return -1 if I2C-failure, 0 if OK
*/
int adp5061_charge_disable(struct adp5061_dev *dev);

/**
* Set charger configuration
*
* @param adp5061 device
* @param new configuration to set
*
* @return -1 if I2C-failure, 0 if OK
*/
int adp5061_set_config(struct adp5061_dev *dev,
        const struct adp5061_config *config);

/**
* Read one 8bit register from charger
*
* @param dev the controller device
* @param addr register address
* @param value pointer to register value
*
* @return error
*/
int adp5061_get_reg(struct adp5061_dev *dev, uint8_t addr,uint8_t *value);

/**
* Write one 8bit charger register
*
* @param dev the charger device
* @param addr register address
* @param value to write
*
* @return error
*/
int adp5061_set_reg(struct adp5061_dev *dev, uint8_t addr,uint8_t value);

/**
* Write number of 8bit charger registers
*
* @param dev the charger device
* @param addr address of first register to write
* @param values to write
* @param number of value to write
*
* @return -1 if I2C-failure, 0 if OK
*/
int adp5061_set_regs(struct adp5061_dev *dev, uint8_t addr,
        const uint8_t *values, int count);

#define ADP5061_ADDR_W                  0x28
#define ADP5061_ADDR_R                  0x29
/* Address used in I2C API */
#define ADP5061_ADDR                    (ADP5061_ADDR_W >> 1)

/* REG_PART_ID 0x00 */

/* MANUF */
#define ADP5061_PART_ID_MANUF_LEN       4
#define ADP5061_PART_ID_MANUF_OFFSET    4
#define ADP5061_PART_ID_MANUF_MASK      (((1 << ADP5061_PART_ID_MANUF_LEN)-1) \
                                            << ADP5061_PART_ID_MANUF_OFFSET)
#define ADP5061_PART_ID_MANUF_GET(a)    ((a & ADP5061_PART_ID_MANUF_MASK) \
                                            >> ADP5061_PART_ID_MANUF_OFFSET)

/* MODEL */
#define ADP5061_PART_ID_MODEL_LEN       4
#define ADP5061_PART_ID_MODEL_OFFSET    0
#define ADP5061_PART_ID_MODEL_MASK      (((1 << ADP5061_PART_ID_MODEL_LEN)-1) \
                                            << ADP5061_PART_ID_MODEL_OFFSET)
#define ADP5061_PART_ID_MODEL_GET(a)    ((a & ADP5061_PART_ID_MODEL_MASK) \
                                            >> ADP5061_PART_ID_MODEL_OFFSET)

/* REG_SILICON_REV 0x01 */

/* REV */
#define ADP5061_SILICON_REV_LEN         4
#define ADP5061_SILICON_REV_OFFSET      0
#define ADP5061_SILICON_REV_MASK        (((1 << ADP5061_SILICON_REV_LEN)-1) \
                                            << ADP5061_SILICON_REV_OFFSET)
#define ADP5061_SILICON_REV_GET(a)      ((a & ADP5061_SILICON_REV_MASK) \
                                            >> ADP5061_SILICON_REV_OFFSET)


/* REG_VIN_PIN_SETTINGS 0x02 */

/* ILIM */
#define ADP5061_VIN_SETTINGS_LEN        4
#define ADP5061_VIN_SETTINGS_OFFSET     0
#define ADP5061_VIN_SETTINGS_MASK       (((1 << ADP5061_VIN_SETTINGS_LEN)-1) \
                                            << ADP5061_VIN_SETTINGS_OFFSET)
#define ADP5061_VIN_SETTINGS_GET(a)     ((a & ADP5061_VIN_SETTINGS_MASK) \
                                            >> ADP5061_VIN_SETTINGS_OFFSET)
#define ADP5061_VIN_SETTINGS_SET(a)     ((a &((1 << ADP5061_VIN_SETTINGS_LEN)-1)) \
                                            << ADP5061_VIN_SETTINGS_OFFSET)
#define ADP5061_VIN_SETTINGS_ILIM_100mA     0x0
#define ADP5061_VIN_SETTINGS_ILIM_150mA     0x1
#define ADP5061_VIN_SETTINGS_ILIM_200mA     0x2
#define ADP5061_VIN_SETTINGS_ILIM_250mA     0x3
#define ADP5061_VIN_SETTINGS_ILIM_300mA     0x4
#define ADP5061_VIN_SETTINGS_ILIM_400mA     0x5
#define ADP5061_VIN_SETTINGS_ILIM_500mA     0x6
#define ADP5061_VIN_SETTINGS_ILIM_600mA     0x7
#define ADP5061_VIN_SETTINGS_ILIM_700mA     0x8
#define ADP5061_VIN_SETTINGS_ILIM_800mA     0x9
#define ADP5061_VIN_SETTINGS_ILIM_900mA     0xA
#define ADP5061_VIN_SETTINGS_ILIM_1000mA    0xB
#define ADP5061_VIN_SETTINGS_ILIM_1200mA    0xC
#define ADP5061_VIN_SETTINGS_ILIM_1500mA    0xD
#define ADP5061_VIN_SETTINGS_ILIM_1800mA    0xE
#define ADP5061_VIN_SETTINGS_ILIM_2100mA    0xF

/* REG_TERM_SETTINGS 0x03 */

/* VTRM */
#define ADP5061_VTRM_LEN            6
#define ADP5061_VTRM_OFFSET         2
#define ADP5061_VTRM_MASK       (((1 << ADP5061_VTRM_LEN)-1) \
                                    << ADP5061_VTRM_OFFSET)
#define ADP5061_VTRM_GET(a)     ((a & ADP5061_VTRM_MASK) \
                                    >> ADP5061_VTRM_OFFSET)
#define ADP5061_VTRM_SET(a)     ((a &((1 << ADP5061_VTRM_LEN)-1)) \
                                    << ADP5061_VTRM_OFFSET)
#define ADP5061_VTRM_3V80           0x0F
#define ADP5061_VTRM_3V82           0x10
#define ADP5061_VTRM_3V84           0x11
#define ADP5061_VTRM_3V86           0x12
#define ADP5061_VTRM_3V88           0x13
#define ADP5061_VTRM_3V90           0x14
#define ADP5061_VTRM_3V92           0x15
#define ADP5061_VTRM_3V94           0x16
#define ADP5061_VTRM_3V96           0x17
#define ADP5061_VTRM_3V98           0x18
#define ADP5061_VTRM_4V00           0x19
#define ADP5061_VTRM_4V02           0x1A
#define ADP5061_VTRM_4V04           0x1B
#define ADP5061_VTRM_4V06           0x1C
#define ADP5061_VTRM_4V08           0x1D
#define ADP5061_VTRM_4V10           0x1E
#define ADP5061_VTRM_4V12           0x1F
#define ADP5061_VTRM_4V14           0x21
#define ADP5061_VTRM_4V16           0x22
#define ADP5061_VTRM_4V18           0x23
#define ADP5061_VTRM_4V20           0x24
#define ADP5061_VTRM_4V24           0x25
#define ADP5061_VTRM_4V26           0x26
#define ADP5061_VTRM_4V28           0x27
#define ADP5061_VTRM_4V30           0x28
#define ADP5061_VTRM_4V32           0x29
#define ADP5061_VTRM_4V34           0x2A
#define ADP5061_VTRM_4V36           0x2B
#define ADP5061_VTRM_4V38           0x2C
#define ADP5061_VTRM_4V40           0x2D
#define ADP5061_VTRM_4V42           0x2E
#define ADP5061_VTRM_4V44           0x2F
#define ADP5061_VTRM_4V46           0x31
#define ADP5061_VTRM_4V48           0x32
#define ADP5061_VTRM_4V50           0x33

/* CHG_VLIM */
#define APD5061_CHG_VLIM_LEN        2
#define APD5061_CHG_VLIM_OFFSET     0
#define ADP5061_CHG_VLIM_MASK       (((1 << APD5061_CHG_VLIM_LEN)-1) \
                                        << APD5061_CHG_VLIM_OFFSET)
#define ADP5061_CHG_VLIM_GET(a)     ((a & ADP5061_CHG_VLIM_MASK) \
                                        >> APD5061_CHG_VLIM_OFFSET)
#define ADP5061_CHG_VLIM_SET(a)     ((a &((1 << APD5061_CHG_VLIM_LEN)-1)) \
                                        << APD5061_CHG_VLIM_OFFSET)
#define ADP5061_CHG_VLIM_3V2        0x0
#define ADP5061_CHG_VLIM_3V4        0x1
#define ADP5061_CHG_VLIM_3V7        0x2
#define ADP5061_CHG_VLIM_3V8        0x3

/* REG_CHARGING_CURRENT 0x04 */

/* ICHG */
#define ADP5061_ICHG_LEN            5
#define ADP5061_ICHG_OFFSET         2
#define ADP5061_ICHG_MASK           (((1 << ADP5061_ICHG_LEN)-1) \
                                        << ADP5061_ICHG_OFFSET)
#define ADP5061_ICHG_GET(a)         ((a & ADP5061_ICHG_MASK) \
                                        >> ADP5061_ICHG_OFFSET)
#define ADP5061_ICHG_SET(a)         ((a &((1 << ADP5061_ICHG_LEN)-1)) \
                                        << ADP5061_ICHG_OFFSET)
#define ADP5061_ICHG_50mA           0x00
#define ADP5061_ICHG_100mA          0x01
#define ADP5061_ICHG_150mA          0x02
#define ADP5061_ICHG_200mA          0x03
#define ADP5061_ICHG_250mA          0x04
#define ADP5061_ICHG_300mA          0x05
#define ADP5061_ICHG_350mA          0x06
#define ADP5061_ICHG_400mA          0x07
#define ADP5061_ICHG_450mA          0x08
#define ADP5061_ICHG_500mA          0x09
#define ADP5061_ICHG_550mA          0x0A
#define ADP5061_ICHG_600mA          0x0B
#define ADP5061_ICHG_650mA          0x0C
#define ADP5061_ICHG_700mA          0x0D
#define ADP5061_ICHG_750mA          0x0E
#define ADP5061_ICHG_800mA          0x0F
#define ADP5061_ICHG_850mA          0x10
#define ADP5061_ICHG_900mA          0x11
#define ADP5061_ICHG_950mA          0x12
#define ADP5061_ICHG_1000mA         0x13
#define ADP5061_ICHG_1050mA         0x14
#define ADP5061_ICHG_1100mA         0x15
#define ADP5061_ICHG_1200mA         0x16
#define ADP5061_ICHG_1300mA         0x17

/* ITRK_DEAD */
#define ADP5061_ITRK_DEAD_LEN       2
#define ADP5061_ITRK_DEAD_OFFSET    0
#define ADP5061_ITRK_DEAD_MASK      (((1 << ADP5061_ITRK_DEAD_LEN)-1) \
                                        << ADP5061_ITRK_DEAD_OFFSET)
#define ADP5061_ITRK_DEAD_GET(a)    ((a & ADP5061_ITRK_DEAD_MASK) \
                                        >> ADP5061_ITRK_DEAD_OFFSET)
#define ADP5061_ITRK_DEAD_SET(a)    ((a &((1 << ADP5061_ITRK_DEAD_LEN)-1)) \
                                        << ADP5061_ITRK_DEAD_OFFSET)
#define ADP5061_ITRK_DEAD_5mA       0x0
#define ADP5061_ITRK_DEAD_10mA      0x1
#define ADP5061_ITRK_DEAD_20mA      0x2
#define ADP5061_ITRK_DEAD_80mA      0x3

/* REG_VOLTAGE_THRES    0x05 */

/* DIS_RCH */
#define ADP5061_VOLTAGE_THRES_DIS_RCH_LEN       1
#define ADP5061_VOLTAGE_THRES_DIS_RCH_OFFSET    7
#define ADP5061_VOLTAGE_THRES_DIS_RCH_MASK      (((1 << ADP5061_VOLTAGE_THRES_DIS_RCH_LEN)-1) \
                                                    << ADP5061_VOLTAGE_THRES_DIS_RCH_OFFSET)
#define ADP5061_VOLTAGE_THRES_DIS_RCH_GET(a)    ((a & ADP5061_VOLTAGE_THRES_DIS_RCH_MASK) \
                                                    >> ADP5061_VOLTAGE_THRES_DIS_RCH_OFFSET)
#define ADP5061_VOLTAGE_THRES_DIS_RCH_SET(a)    ((a &((1 << ADP5061_VOLTAGE_THRES_DIS_RCH_LEN)-1)) \
                                                    << ADP5061_VOLTAGE_THRES_DIS_RCH_OFFSET)
#define ADP5061_VOLTAGE_THRES_RCH_EN            0x0
#define ADP5061_VOLTAGE_THRES_RCH_DIS           0x1

/* VRCH */
#define ADP5061_VOLTAGE_THRES_VRCH_LEN          2
#define ADP5061_VOLTAGE_THRES_VRCH_OFFSET       5
#define ADP5061_VOLTAGE_THRES_VRCH_MASK         (((1 << ADP5061_VOLTAGE_THRES_VRCH_LEN)-1) \
                                                    << ADP5061_VOLTAGE_THRES_VRCH_OFFSET)
#define ADP5061_VOLTAGE_THRES_VRCH_GET(a)       ((a & ADP5061_VOLTAGE_THRES_VTRK_DEAD_MASK) \
                                                    >> ADP5061_VOLTAGE_THRES_VTRK_DEAD_OFFSET)
#define ADP5061_VOLTAGE_THRES_VRCH_SET(a)       ((a & ((1 << ADP5061_VOLTAGE_THRES_VRCH_LEN)-1)) \
                                                    << ADP5061_VOLTAGE_THRES_VRCH_OFFSET)
#define ADP5061_VOLTAGE_THRES_VRCH_80mV         0x00
#define ADP5061_VOLTAGE_THRES_VRCH_140mV        0x01
#define ADP5061_VOLTAGE_THRES_VRCH_200mV        0x02
#define ADP5061_VOLTAGE_THRES_VRCH_260mV        0x03

/* VTRK_DEAD */
#define ADP5061_VOLTAGE_THRES_VTRK_DEAD_LEN     2
#define ADP5061_VOLTAGE_THRES_VTRK_DEAD_OFFSET  3
#define ADP5061_VOLTAGE_THRES_VTRK_DEAD_MASK    (((1 << ADP5061_VOLTAGE_THRES_VTRK_DEAD_LEN)-1) \
                                                    << ADP5061_VOLTAGE_THRES_VTRK_DEAD_OFFSET)
#define ADP5061_VOLTAGE_THRES_VTRK_DEAD_GET (a) ((a & ADP5061_VOLTAGE_THRES_VTRK_DEAD_MASK) \
                                                    >> ADP5061_VOLTAGE_THRES_VTRK_DEAD_OFFSET)
#define ADP5061_VOLTAGE_THRES_VTRK_DEAD_SET(a)  ((a & ((1 << ADP5061_VOLTAGE_THRES_VTRK_DEAD_LEN)-1)) \
                                                    << ADP5061_VOLTAGE_THRES_VTRK_DEAD_OFFSET)
#define ADP5061_VOLTAGE_THRES_VTRK_DEAD_2V0     0x00
#define ADP5061_VOLTAGE_THRES_VTRK_DEAD_2V5     0x01
#define ADP5061_VOLTAGE_THRES_VTRK_DEAD_2V6     0x02
#define ADP5061_VOLTAGE_THRES_VTRK_DEAD_2V9     0x03

/* VWEAK */
#define ADP5061_VOLTAGE_THRES_VWEAK_LEN             3
#define ADP5061_VOLTAGE_THRES_VWEAK_OFFSET          0
#define ADP5061_VOLTAGE_THRES_VWEAK_MASK            (((1 << ADP5061_VOLTAGE_THRES_VWEAK_LEN)-1) \
                                                        << ADP5061_VOLTAGE_THRES_VWEAK_OFFSET)
#define ADP5061_VOLTAGE_THRES_VWEAK_GET(a)          ((a & ADP5061_VOLTAGE_THRES_VWEAK_MASK) \
                                                        >> ADP5061_VOLTAGE_THRES_VWEAK_OFFSET)
#define ADP5061_VOLTAGE_THRES_VWEAK_SET(a)          ((a & ((1 << ADP5061_VOLTAGE_THRES_VWEAK_LEN)-1)) \
                                                        << ADP5061_VOLTAGE_THRES_VWEAK_OFFSET)
#define ADP5061_VOLTAGE_THRES_VWEAK_2V7             0x0
#define ADP5061_VOLTAGE_THRES_VWEAK_2V8             0x1
#define ADP5061_VOLTAGE_THRES_VWEAK_2V9             0x2
#define ADP5061_VOLTAGE_THRES_VWEAK_3V0             0x3
#define ADP5061_VOLTAGE_THRES_VWEAK_3V1             0x4
#define ADP5061_VOLTAGE_THRES_VWEAK_3V2             0x5
#define ADP5061_VOLTAGE_THRES_VWEAK_3V3             0x6
#define ADP5061_VOLTAGE_THRES_VWEAK_3V4             0x7

/* REG_TIMER_SETTINGS 0x06 */

/* EN_TEND */
#define ADP5061_TIMER_SETTINGS_EN_TEND_LEN          1
#define ADP5061_TIMER_SETTINGS_EN_TEND_OFFSET       5
#define ADP5061_TIMER_SETTINGS_EN_TEND_MASK         (((1 << ADP5061_TIMER_SETTINGS_EN_TEND_LEN)-1) \
                                                        << ADP5061_TIMER_SETTINGS_EN_TEND_OFFSET)
#define ADP5061_TIMER_SETTINGS_EN_TEND_GET(a)       ((a & ADP5061_TIMER_SETTINGS_EN_TEND_MASK) \
                                                        >> ADP5061_TIMER_SETTINGS_EN_TEND_OFFSET)
#define ADP5061_TIMER_SETTINGS_EN_TEND_SET(a)       ((a & ((1 << ADP5061_TIMER_SETTINGS_EN_TEND_LEN)-1)) \
                                                        << ADP5061_TIMER_SETTINGS_EN_TEND_OFFSET)

/* EN_CHG_TIMER */
#define ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_LEN     1
#define ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_OFFSET  4
#define ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_MASK    (((1 << ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_LEN)-1) \
                                                        << ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_OFFSET)
#define ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_GET(a)  ((a & ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_MASK) \
                                                        >> ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_OFFSET)
#define ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_SET(a)  ((a & ((1 << ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_LEN)-1)) \
                                                        << ADP5061_TIMER_SETTINGS_EN_CHG_TIMER_OFFSET)
/* CHG_TMR_PERIOD */
#define ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_LEN    1
#define ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_OFFSET 3
#define ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_MASK   (((1 << ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_LEN)-1) \
                                                    << ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_OFFSET)
#define ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_GET(a) ((a & ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_MASK) \
                                                        >> ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_OFFSET)
#define ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_SET(a) ((a & ((1 << ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_LEN)-1)) \
                                                        << ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_OFFSET)
#define ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_30s    0x0
#define ADP5061_TIMER_SETTINGS_EN_TMR_PERIOD_60s    0x1

/* EN_WD */
#define ADP5061_TIMER_SETTINGS_EN_WD_LEN            1
#define ADP5061_TIMER_SETTINGS_EN_WD_OFFSET         2
#define ADP5061_TIMER_SETTINGS_EN_WD_MASK           (((1 << ADP5061_TIMER_SETTINGS_EN_WD_LEN)-1) \
                                                        << ADP5061_TIMER_SETTINGS_EN_WD_OFFSET)
#define ADP5061_TIMER_SETTINGS_EN_WD_GET(a)         ((a & ADP5061_TIMER_SETTINGS_EN_WD_MASK) \
                                                        >> ADP5061_TIMER_SETTINGS_EN_WD_OFFSET)
#define ADP5061_TIMER_SETTINGS_EN_WD_SET(a)         ((a & ((1 << ADP5061_TIMER_SETTINGS_EN_WD_LEN)-1)) \
                                                        << ADP5061_TIMER_SETTINGS_EN_WD_OFFSET)

/* WD_PERIOD */
#define ADP5061_TIMER_SETTINGS_WD_PERIOD_LEN        1
#define ADP5061_TIMER_SETTINGS_WD_PERIOD_OFFSET     1
#define ADP5061_TIMER_SETTINGS_WD_PERIOD_MASK       (((1 << ADP5061_TIMER_SETTINGS_WD_PERIOD_LEN)-1) \
                                                        << ADP5061_TIMER_SETTINGS_WD_PERIOD_OFFSET)
#define ADP5061_TIMER_SETTINGS_WD_PERIOD_GET(a)     ((a & ADP5061_TIMER_SETTINGS_WD_PERIOD_MASK) \
                                                        >> ADP5061_TIMER_SETTINGS_WD_PERIOD_OFFSET)
#define ADP5061_TIMER_SETTINGS_WD_PERIOD_SET(a)     ((a & ((1 << ADP5061_TIMER_SETTINGS_WD_PERIOD_LEN)-1)) \
                                                        << ADP5061_TIMER_SETTINGS_WD_PERIOD_OFFSET)

/* RESET_WD */
#define ADP5061_TIMER_SETTINGS_WD_PERIOD_32s        0x0
#define ADP5061_TIMER_SETTINGS_WD_PERIOD_64s        0x1

#define ADP5061_TIMER_SETTINGS_RESET_WD_LEN         1
#define ADP5061_TIMER_SETTINGS_RESET_WD_OFFSET      0
#define ADP5061_TIMER_SETTINGS_RESET_WD_SET(a)      ((a & ((1 << ADP5061_TIMER_SETTINGS_RESET_WD_LEN)-1)) \
                                                        << ADP5061_TIMER_SETTINGS_RESET_WD_OFFSET)

/* REG_FUNC_SETTINGS_1 0x07 */

/* DIS_IC1 */
#define ADP5061_FUNC_SETTINGS_1_DIS_IC1_LEN         1
#define ADP5061_FUNC_SETTINGS_1_DIS_IC1_OFFSET      6
#define ADP5061_FUNC_SETTINGS_1_DIS_IC1_MASK        (((1 << ADP5061_FUNC_SETTINGS_1_DIS_IC1_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_1_DIS_IC1_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_DIS_IC1_GET(a)      ((a & ADP5061_FUNC_SETTINGS_1_DIS_IC1_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_1_DIS_IC1_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_DIS_IC1_SET(a)      ((a & ((1 << ADP5061_FUNC_SETTINGS_1_DIS_IC1_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_1_DIS_IC1_OFFSET)

/* EN_BMON */
#define ADP5061_FUNC_SETTINGS_1_EN_BMON_LEN         1
#define ADP5061_FUNC_SETTINGS_1_EN_BMON_OFFSET      5
#define ADP5061_FUNC_SETTINGS_1_EN_BMON_MASK        (((1 << ADP5061_FUNC_SETTINGS_1_EN_BMON_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_1_EN_BMON_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_EN_BMON_GET(a)      ((a & ADP5061_FUNC_SETTINGS_1_EN_BMON_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_1_EN_BMON_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_EN_BMON_SET(a)      ((a & ((1 << ADP5061_FUNC_SETTINGS_1_EN_BMON_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_1_EN_BMON_OFFSET)

/* EN_THR */
#define ADP5061_FUNC_SETTINGS_1_EN_THR_LEN          1
#define ADP5061_FUNC_SETTINGS_1_EN_THR_OFFSET       4
#define ADP5061_FUNC_SETTINGS_1_EN_THR_MASK         (((1 << ADP5061_FUNC_SETTINGS_1_EN_THR_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_1_EN_THR_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_EN_THR_GET(a)       ((a & ADP5061_FUNC_SETTINGS_1_EN_THR_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_1_EN_THR_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_EN_THR_SET(a)       ((a & ((1 << ADP5061_FUNC_SETTINGS_1_EN_THR_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_1_EN_THR_OFFSET)

/* DIS_LDO */
#define ADP5061_FUNC_SETTINGS_1_DIS_LDO_LEN         1
#define ADP5061_FUNC_SETTINGS_1_DIS_LDO_OFFSET      3
#define ADP5061_FUNC_SETTINGS_1_DIS_LDO_MASK        (((1 << ADP5061_FUNC_SETTINGS_1_DIS_LDO_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_1_DIS_LDO_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_DIS_LDO_GET(a)      ((a & ADP5061_FUNC_SETTINGS_1_DIS_LDO_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_1_DIS_LDO_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_DIS_LDO_SET(a)      ((a & ((1 << ADP5061_FUNC_SETTINGS_1_DIS_LDO_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_1_DIS_LDO_OFFSET)

/* EN_EOC */
#define ADP5061_FUNC_SETTINGS_1_EN_EOC_LEN          1
#define ADP5061_FUNC_SETTINGS_1_EN_EOC_OFFSET       2
#define ADP5061_FUNC_SETTINGS_1_EN_EOC_MASK         (((1 << ADP5061_FUNC_SETTINGS_1_EN_EOC_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_1_EN_EOC_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_EN_EOC_GET(a)       ((a & ADP5061_FUNC_SETTINGS_1_EN_EOC_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_1_EN_EOC_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_EN_EOC_SET(a)       ((a & ((1 << ADP5061_FUNC_SETTINGS_1_EN_EOC_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_1_EN_EOC_OFFSET)

/* EN_CHG */
#define ADP5061_FUNC_SETTINGS_1_EN_CHG_LEN          1
#define ADP5061_FUNC_SETTINGS_1_EN_CHG_OFFSET       0
#define ADP5061_FUNC_SETTINGS_1_EN_CHG_MASK         (((1 << ADP5061_FUNC_SETTINGS_1_EN_CHG_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_1_EN_CHG_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_EN_CHG_GET(a)       ((a & ADP5061_FUNC_SETTINGS_1_EN_CHG_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_1_EN_CHG_OFFSET)
#define ADP5061_FUNC_SETTINGS_1_EN_CHG_SET(a)       ((a & ((1 << ADP5061_FUNC_SETTINGS_1_EN_CHG_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_1_EN_CHG_OFFSET)

/* REG_FUNC_SETTINGS_2 0x08 */

/* EN_JEITA */
#define ADP5061_FUNC_SETTINGS_2_EN_JEITA_LEN        1
#define ADP5061_FUNC_SETTINGS_2_EN_JEITA_OFFSET     7
#define ADP5061_FUNC_SETTINGS_2_EN_JEITA_MASK       (((1 << ADP5061_FUNC_SETTINGS_2_EN_JEITA_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_2_EN_JEITA_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_EN_JEITA_GET(a)     ((a & ADP5061_FUNC_SETTINGS_2_EN_JEITA_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_2_EN_JEITA_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_EN_JEITA_SET(a)     ((a & ((1 << ADP5061_FUNC_SETTINGS_2_EN_JEITA_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_2_EN_JEITA_OFFSET)

/* JEITA_SELECT */
#define ADP5061_FUNC_SETTINGS_2_JEITA_SEL_LEN       1
#define ADP5061_FUNC_SETTINGS_2_JEITA_SEL_OFFSET    6
#define ADP5061_FUNC_SETTINGS_2_JEITA_SEL_MASK      (((1 << ADP5061_FUNC_SETTINGS_2_JEITA_SEL_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_2_JEITA_SEL_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_JEITA_SEL_GET(a)    ((a & ADP5061_FUNC_SETTINGS_2_JEITA_SEL_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_2_JEITA_SEL_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_JEITA_SEL_SET(a)    ((a & ((1 << ADP5061_FUNC_SETTINGS_2_JEITA_SEL_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_2_JEITA_SEL_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_JEITA_SEL_1         0x0
#define ADP5061_FUNC_SETTINGS_2_JEITA_SEL_2         0x1

/* EN_CHG_VLIM */
#define ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_LEN     1
#define ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_OFFSET  5
#define ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_MASK    (((1 << ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_GET(a)  ((a & ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_SET(a)  ((a & ((1 << ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_2_EN_CHG_VLIM_OFFSET)

/* IDEAL_DIODE */
#define ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_LEN     2
#define ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_OFFSET  3
#define ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_MASK    (((1 << ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_GET(a)  ((a & ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_SET(a)  ((a & ((1 << ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_2_IDEAL_DIODE_OFFSET)

/* VSYSTEM */
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_LEN         3
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_OFFSET      0
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_MASK        (((1 << ADP5061_FUNC_SETTINGS_2_VSYSTEM_LEN)-1) \
                                                        << ADP5061_FUNC_SETTINGS_2_VSYSTEM_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_GET(a)      ((a & ADP5061_FUNC_SETTINGS_2_VSYSTEM_MASK) \
                                                        >> ADP5061_FUNC_SETTINGS_2_VSYSTEM_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_SET(a)      ((a & ((1 << ADP5061_FUNC_SETTINGS_2_VSYSTEM_LEN)-1)) \
                                                        << ADP5061_FUNC_SETTINGS_2_VSYSTEM_OFFSET)
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_4V3         0x0
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_4V4         0x1
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_4V5         0x2
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_4V6         0x3
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_4V7         0x4
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_4V8         0x5
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_4V9         0x6
#define ADP5061_FUNC_SETTINGS_2_VSYSTEM_5V0         0x7

/* REG_INT_EN   0x09 */

#define ADP5061_INT_EN_ALL                  0x7F

/* EN_THERM_LIMIT INT */
#define ADP5061_INT_EN_THERM_LIM_LEN        1
#define ADP5061_INT_EN_THERM_LIM_OFFSET     6
#define ADP5061_INT_EN_THERM_LIM_MASK       (((1 << ADP5061_INT_EN_THERM_LIM_LEN)-1) \
                                                    << ADP5061_INT_EN_THERM_LIM_OFFSET)
#define ADP5061_INT_EN_THERM_LIM_GET(a)     ((a & ADP5061_INT_EN_THERM_LIM_MASK) \
                                                    >> ADP5061_INT_EN_THERM_LIM_OFFSET)
#define ADP5061_INT_EN_THERM_LIM_SET(a)     ((a & ((1 << ADP5061_INT_EN_THERM_LIM_LEN)-1)) \
                                                    << ADP5061_INT_EN_THERM_LIM_OFFSET)

/* EN_WD INT */
#define ADP5061_INT_EN_WD_LEN               1
#define ADP5061_INT_EN_WD_OFFSET            5
#define ADP5061_INT_EN_WD_MASK              (((1 << ADP5061_INT_EN_WD_LEN)-1) \
                                                << ADP5061_INT_EN_WD_OFFSET)
#define ADP5061_INT_EN_WD_GET(a)            ((a & ADP5061_INT_EN_WD_MASK) \
                                                >> ADP5061_INT_EN_WD_OFFSET)
#define ADP5061_INT_EN_WD_SET(a)            ((a & ((1 << ADP5061_INT_EN_WD_LEN)-1)) \
                                                << ADP5061_INT_EN_WD_OFFSET)

/* EN_TSD INT */
#define ADP5061_INT_EN_TSD_LEN              1
#define ADP5061_INT_EN_TSD_OFFSET           4
#define ADP5061_INT_EN_TSD_MASK             (((1 << ADP5061_INT_EN_TSD_LEN)-1) \
                                                << ADP5061_INT_EN_TSD_OFFSET)
#define ADP5061_INT_EN_TSD_GET(a)           ((a & ADP5061_INT_EN_TSD_MASK) \
                                                >> ADP5061_INT_EN_TSD_OFFSET)
#define ADP5061_INT_EN_TSD_SET(a)           ((a & ((1 << ADP5061_INT_EN_TSD_LEN)-1)) \
                                                << ADP5061_INT_EN_TSD_OFFSET)

/* EN_THR INT */
#define ADP5061_INT_EN_THR_LEN              1
#define ADP5061_INT_EN_THR_OFFSET           3
#define ADP5061_INT_EN_THR_MASK             (((1 << ADP5061_INT_EN_THR_LEN)-1) \
                                                << ADP5061_INT_EN_THR_OFFSET)
#define ADP5061_INT_EN_THR_GET(a)           ((a & ADP5061_INT_EN_THR_MASK) \
                                                >> ADP5061_INT_EN_THR_OFFSET)
#define ADP5061_INT_EN_THR_SET(a)           ((a & ((1 << ADP5061_INT_EN_THR_LEN)-1)) \
                                                << ADP5061_INT_EN_THR_OFFSET)

/* EN_BAT INT */
#define ADP5061_INT_EN_BAT_LEN              1
#define ADP5061_INT_EN_BAT_OFFSET           2
#define ADP5061_INT_EN_BAT_MASK             (((1 << ADP5061_INT_EN_BAT_LEN)-1) \
                                                << ADP5061_INT_EN_BAT_OFFSET)
#define ADP5061_INT_EN_BAT_GET(a)           ((a & ADP5061_INT_EN_BAT_MASK) \
                                                >> ADP5061_INT_EN_BAT_OFFSET)
#define ADP5061_INT_EN_BAT_SET(a)           ((a & ((1 << ADP5061_INT_EN_BAT_LEN)-1)) \
                                                << ADP5061_INT_EN_BAT_OFFSET)

/* EN_CHG INT */
#define ADP5061_INT_EN_CHG_LEN              1
#define ADP5061_INT_EN_CHG_OFFSET           1
#define ADP5061_INT_EN_CHG_MASK             (((1 << ADP5061_INT_EN_CHG_LEN)-1) \
                                                << ADP5061_INT_EN_CHG_OFFSET)
#define ADP5061_INT_EN_CHG_GET(a)           ((a & ADP5061_INT_EN_CHG_MASK) \
                                                >> ADP5061_INT_EN_CHG_OFFSET)
#define ADP5061_INT_EN_CHG_SET(a)           ((a & ((1 << ADP5061_INT_EN_CHG_LEN)-1)) \
                                                << ADP5061_INT_EN_CHG_OFFSET)

/* EN_VIN INT */
#define ADP5061_INT_EN_VIN_LEN              1
#define ADP5061_INT_EN_VIN_OFFSET           0
#define ADP5061_INT_EN_VIN_MASK             (((1 << ADP5061_INT_EN_VIN_LEN)-1) \
                                                << ADP5061_INT_EN_VIN_OFFSET)
#define ADP5061_INT_EN_VIN_GET(a)           ((a & ADP5061_INT_EN_VIN_MASK) \
                                                >> ADP5061_INT_EN_VIN_OFFSET)
#define ADP5061_INT_EN_VIN_SET(a)           ((a & ((1 << ADP5061_INT_EN_VIN_LEN)-1)) \
                                                << ADP5061_INT_EN_VIN_OFFSET)

/* REG_INT_ACTIVE        0x0A; */

/* THERM_LIM_INT */
#define ADP5061_INT_ACTIVE_THERM_LIM_LEN        1
#define ADP5061_INT_ACTIVE_THERM_LIM_OFFSET     6
#define ADP5061_INT_ACTIVE_THERM_LIM_MASK       (((1 << ADP5061_INT_ACTIVE_THERM_LIM_LEN)-1) \
                                                    << ADP5061_INT_ACTIVE_THERM_LIM_OFFSET)
#define ADP5061_INT_ACTIVE_THERM_LIM_GET(a)     ((a & ADP5061_INT_ACTIVE_THERM_LIM_MASK) \
                                                    >> ADP5061_INT_ACTIVE_THERM_LIM_OFFSET)

/* WD_INT */
#define ADP5061_INT_ACTIVE_WD_LEN               1
#define ADP5061_INT_ACTIVE_WD_OFFSET            5
#define ADP5061_INT_ACTIVE_WD_MASK              (((1 << ADP5061_INT_ACTIVE_WD_LEN)-1) \
                                                    << ADP5061_INT_ACTIVE_WD_OFFSET)
#define ADP5061_INT_ACTIVE_WD_GET(a)            ((a & ADP5061_INT_ACTIVE_WD_MASK) \
                                                    >> ADP5061_INT_ACTIVE_WD_OFFSET)

/* TSD_INT */
#define ADP5061_INT_ACTIVE_TSD_LEN              1
#define ADP5061_INT_ACTIVE_TSD_OFFSET           4
#define ADP5061_INT_ACTIVE_TSD_MASK             (((1 << ADP5061_INT_ACTIVE_TSD_LEN)-1) \
                                                    << ADP5061_INT_ACTIVE_TSD_OFFSET)
#define ADP5061_INT_ACTIVE_TSD_GET(a)           ((a & ADP5061_INT_ACTIVE_TSD_MASK) \
                                                    >> ADP5061_INT_ACTIVE_TSD_OFFSET)

/* THR INT */
#define ADP5061_INT_ACTIVE_THR_LEN              1
#define ADP5061_INT_ACTIVE_THR_OFFSET           3
#define ADP5061_INT_ACTIVE_THR_MASK             (((1 << ADP5061_INT_ACTIVE_THR_LEN)-1) \
                                                    << ADP5061_INT_ACTIVE_THR_OFFSET)
#define ADP5061_INT_ACTIVE_THR_GET(a)           ((a & ADP5061_INT_ACTIVE_THR_MASK) \
                                                    >> ADP5061_INT_ACTIVE_THR_OFFSET)
/* BAT_INT */
#define ADP5061_INT_ACTIVE_BAT_LEN              1
#define ADP5061_INT_ACTIVE_BAT_OFFSET           2
#define ADP5061_INT_ACTIVE_BAT_MASK             (((1 << ADP5061_INT_ACTIVE_BAT_LEN)-1) \
                                                    << ADP5061_INT_ACTIVE_BAT_OFFSET)
#define ADP5061_INT_ACTIVE_BAT_GET(a)           ((a & ADP5061_INT_ACTIVE_BAT_MASK) \
                                                    >> ADP5061_INT_ACTIVE_BAT_OFFSET)
/* CHG_INT */
#define ADP5061_INT_ACTIVE_CHG_LEN              1
#define ADP5061_INT_ACTIVE_CHG_OFFSET           1
#define ADP5061_INT_ACTIVE_CHG_MASK             (((1 << ADP5061_INT_ACTIVE_CHG_LEN)-1) \
                                                    << ADP5061_INT_ACTIVE_CHG_OFFSET)
#define ADP5061_INT_ACTIVE_CHG_GET(a)           ((a & ADP5061_INT_ACTIVE_CHG_MASK) \
                                                    >> ADP5061_INT_ACTIVE_CHG_OFFSET)
/* VIN_INT */
#define ADP5061_INT_ACTIVE_VIN_LEN              1
#define ADP5061_INT_ACTIVE_VIN_OFFSET           0
#define ADP5061_INT_ACTIVE_VIN_MASK             (((1 << ADP5061_INT_ACTIVE_VIN_LEN)-1) \
                                                    << ADP5061_INT_ACTIVE_VIN_OFFSET)
#define ADP5061_INT_ACTIVE_VIN_GET(a)           ((a & ADP5061_INT_ACTIVE_VIN_MASK) \
                                                    >> ADP5061_INT_ACTIVE_VIN_OFFSET)


/* REG_CHARGER_STATUS_1 0x0B */

/* VIN_OV */
#define ADP5061_CHG_STATUS_1_VIN_OV_LEN         1
#define ADP5061_CHG_STATUS_1_VIN_OV_OFFSET      7
#define ADP5061_CHG_STATUS_1_VIN_OV_MASK        (((1 << ADP5061_CHG_STATUS_1_VIN_OV_LEN)-1) \
                                                    << ADP5061_CHG_STATUS_1_VIN_OV_OFFSET)
#define ADP5061_CHG_STATUS_1_VIN_OV_GET(a)      ((a & ADP5061_CHG_STATUS_1_VIN_OV_MASK) \
                                                    >> ADP5061_CHG_STATUS_1_VIN_OV_OFFSET)

/* VIN_OK */
#define ADP5061_CHG_STATUS_1_VIN_OK_LEN         1
#define ADP5061_CHG_STATUS_1_VIN_OK_OFFSET      6
#define ADP5061_CHG_STATUS_1_VIN_OK_MASK        (((1 << ADP5061_CHG_STATUS_1_VIN_OK_LEN)-1) \
                                                    << ADP5061_CHG_STATUS_1_VIN_OK_OFFSET)
#define ADP5061_CHG_STATUS_1_VIN_OK_GET(a)      ((a & ADP5061_CHG_STATUS_1_VIN_OK_MASK) \
                                                    >> ADP5061_CHG_STATUS_1_VIN_OK_OFFSET)

/* VIN_ILIM */
#define ADP5061_CHG_STATUS_1_VIN_ILIM_LEN       1
#define ADP5061_CHG_STATUS_1_VIN_ILIM_OFFSET    5
#define ADP5061_CHG_STATUS_1_VIN_ILIM_MASK      (((1 << ADP5061_CHG_STATUS_1_VIN_ILIM_LEN)-1) \
                                                    << ADP5061_CHG_STATUS_1_VIN_ILIM_OFFSET)
#define ADP5061_CHG_STATUS_1_VIN_ILIM_GET(a)    ((a & ADP5061_CHG_STATUS_1_VIN_ILIM_MASK) \
                                                    >> ADP5061_CHG_STATUS_1_VIN_ILIM_OFFSET)

/* THERM_LIM */
#define ADP5061_CHG_STATUS_1_THERM_LIM_LEN      1
#define ADP5061_CHG_STATUS_1_THERM_LIM_OFFSET   4
#define ADP5061_CHG_STATUS_1_THERM_LIM_MASK     (((1 << ADP5061_CHG_STATUS_1_THERM_LIM_LEN)-1) \
                                                    << ADP5061_CHG_STATUS_1_THERM_LIM_OFFSET)
#define ADP5061_CHG_STATUS_1_THERM_LIM_GET(a)   ((a & ADP5061_CHG_STATUS_1_THERM_LIM_MASK) \
                                                    >> ADP5061_CHG_STATUS_1_THERM_LIM_OFFSET)

/* CHDONE */
#define ADP5061_CHG_STATUS_1_CHDONE_LEN         1
#define ADP5061_CHG_STATUS_1_CHDONE_OFFSET      3
#define ADP5061_CHG_STATUS_1_CHDONE_MASK        (((1 << ADP5061_CHG_STATUS_1_CHDONE_LEN)-1) \
                                                    << ADP5061_CHG_STATUS_1_CHDONE_OFFSET)
#define ADP5061_CHG_STATUS_1_CHDONE_GET(a)      ((a & ADP5061_CHG_STATUS_1_CHDONE_MASK) \
                                                    >> ADP5061_CHG_STATUS_1_CHDONE_OFFSET)

/* CHARGER_STATUS */
#define ADP5061_CHG_STATUS_1_LEN                3
#define ADP5061_CHG_STATUS_1_OFFSET             0
#define ADP5061_CHG_STATUS_1_MASK               (((1 << ADP5061_CHG_STATUS_1_LEN)-1) \
                                                    << ADP5061_CHG_STATUS_1_OFFSET)
#define ADP5061_CHG_STATUS_1_GET(a)             ((a & ADP5061_CHG_STATUS_1_MASK) \
                                                    >> ADP5061_CHG_STATUS_1_OFFSET)
#define ADP5061_CHG_STATUS_OFF                  0x0
#define ADP5061_CHG_STATUS_TCK_CHG              0x1
#define ADP5061_CHG_STATUS_FAST_CHG_CC          0x2
#define ADP5061_CHG_STATUS_FAST_CHG_CV          0x3
#define ADP5061_CHG_STATUS_CHG_COMPLETE         0x4
#define ADP5061_CHG_STATUS_LDO_MODE             0x5
#define ADP5061_CHG_STATUS_TCK_EXP              0x6
#define ADP5061_CHG_STATUS_BAT_DET              0x7

/* REG_CHARGER_STATUS_2 0x0C */

/* THR_STATUS */
#define ADP5061_CHG_STATUS_2_THR_LEN            3
#define ADP5061_CHG_STATUS_2_THR_OFFSET         5
#define ADP5061_CHG_STATUS_2_THR_MASK           (((1 << ADP5061_CHG_STATUS_2_THR_LEN)-1) \
                                                    << ADP5061_CHG_STATUS_2_THR_OFFSET)
#define ADP5061_CHG_STATUS_2_THR_GET(a)         ((a & ADP5061_CHG_STATUS_2_THR_MASK) \
                                                    >> ADP5061_CHG_STATUS_2_THR_OFFSET)
#define ADP5061_CHG_STATUS_2_THR_OFF            0x0
#define ADP5061_CHG_STATUS_2_THR_COLD           0x1
#define ADP5061_CHG_STATUS_2_THR_COOL           0x2
#define ADP5061_CHG_STATUS_2_THR_WARM           0x3
#define ADP5061_CHG_STATUS_2_THR_HOT            0x4
#define ADP5061_CHG_STATUS_2_THR_OK             0x7

/* RCH_LIM_INFO */
#define ADP5061_CHG_STATUS_2_RCH_LIM_LEN        1
#define ADP5061_CHG_STATUS_2_RCH_LIM_OFFSET     3
#define ADP5061_CHG_STATUS_2_RCH_LIM_MASK       (((1 << ADP5061_CHG_STATUS_2_RCH_LIM_LEN)-1) \
                                                    << ADP5061_CHG_STATUS_2_RCH_LIM_OFFSET)
#define ADP5061_CHG_STATUS_2_RCH_LIM_GET(a)     ((a & ADP5061_CHG_STATUS_2_RCH_LIM_MASK) \
                                                    >> ADP5061_CHG_STATUS_2_RCH_LIM_OFFSET)

/* BATTERY_STATUS */
#define ADP5061_CHG_STATUS_2_BATT_LEN           3
#define ADP5061_CHG_STATUS_2_BATT_OFFSET        0
#define ADP5061_CHG_STATUS_2_BATT_MASK          (((1 << ADP5061_CHG_STATUS_2_BATT_LEN)-1) \
                                                    << ADP5061_CHG_STATUS_2_BATT_OFFSET)
#define ADP5061_CHG_STATUS_2_BATT_GET(a)        ((a & ADP5061_CHG_STATUS_2_BATT_MASK) \
                                                    >> ADP5061_CHG_STATUS_2_BATT_OFFSET)

/* REG_FAULT_REGISTER    0x0D */

/* BAT_SHR */
#define ADP5061_FAULT_BAT_SHR_LEN           1
#define ADP5061_FAULT_BAT_SHR_OFFSET        3
#define ADP5061_FAULT_BAT_SHR_MASK          (((1 << ADP5061_FAULT_BAT_SHR_LEN)-1) \
                                                << ADP5061_FAULT_BAT_SHR_OFFSET)
#define ADP5061_FAULT_BAT_SHR_GET(a)        ((a & ADP5061_FAULT_BAT_SHR_MASK) \
                                                >> ADP5061_FAULT_BAT_SHR_OFFSET)
#define ADP5061_FAULT_BAT_SHR_SET(a)        ((a & ((1 << ADP5061_FAULT_BAT_SHR_LEN)-1)) \
                                                << ADP5061_FAULT_BAT_SHR_OFFSET)

/* TSD_130C */
#define ADP5061_TSD_130C_LEN                1
#define ADP5061_TSD_130C_OFFSET             1
#define ADP5061_TSD_130C_MASK               (((1 << ADP5061_TSD_130C_LEN)-1) \
                                                << ADP5061_TSD_130C_OFFSET)
#define ADP5061_TSD_130C_GET(a)             ((a & ADP5061_TSD_130C_MASK) \
                                                >> ADP5061_TSD_130C_OFFSET)
#define ADP5061_TSD_130C_SET(a)             ((a & ((1 << ADP5061_TSD_130C_LEN)-1)) \
                                                << ADP5061_TSD_130C_OFFSET)

/* TSD_140C */
#define ADP5061_TSD_140C_LEN                1
#define ADP5061_TSD_140C_OFFSET             0
#define ADP5061_TSD_140C_MASK               (((1 << ADP5061_TSD_140C_LEN)-1) \
                                                << ADP5061_TSD_140C_OFFSET)
#define ADP5061_TSD_140C_GET(a)             ((a & ADP5061_TSD_140C_MASK) \
                                                >> ADP5061_TSD_140C_OFFSET)
#define ADP5061_TSD_140C_SET(a)             ((a & ((1 << ADP5061_TSD_140C_LEN)-1)) \
                                                << ADP5061_TSD_140C_OFFSET)

/* REG_BATT_SHORT        0x10 */

/* TBAT_SHR */
#define ADP5061_BATT_SHORT_T_THR_LEN        3
#define ADP5061_BATT_SHORT_T_THR_OFFSET     5
#define ADP5061_BATT_SHORT_T_THR_MASK       (((1 << ADP5061_BATT_SHORT_T_THR_LEN)-1) \
                                                << ADP5061_BATT_SHORT_T_THR_OFFSET)
#define ADP5061_BATT_SHORT_T_THR_GET(a)     ((a & ADP5061_BATT_SHORT_T_THR_MASK) \
                                                >> ADP5061_BATT_SHORT_T_THR_OFFSET)
#define ADP5061_BATT_SHORT_T_THR_SET(a)     ((a & ((1 << ADP5061_BATT_SHORT_T_THR_LEN)-1)) \
                                                << ADP5061_BATT_SHORT_T_THR_OFFSET)
#define ADP5061_BATT_SHORT_T_THR_1S         0x0
#define ADP5061_BATT_SHORT_T_THR_2S         0x1
#define ADP5061_BATT_SHORT_T_THR_4S         0x2
#define ADP5061_BATT_SHORT_T_THR_10S        0x3
#define ADP5061_BATT_SHORT_T_THR_30S        0x4
#define ADP5061_BATT_SHORT_T_THR_60S        0x5
#define ADP5061_BATT_SHORT_T_THR_120S       0x6
#define ADP5061_BATT_SHORT_T_THR_180S       0x7

/* VBAT_SHR */
#define ADP5061_BATT_SHORT_V_THR_LEN        3
#define ADP5061_BATT_SHORT_V_THR_OFFSET     0
#define ADP5061_BATT_SHORT_V_THR_MASK       (((1 << ADP5061_BATT_SHORT_V_THR_LEN)-1) \
                                                << ADP5061_BATT_SHORT_V_THR_OFFSET)
#define ADP5061_BATT_SHORT_V_THR_GET(a)     ((a & ADP5061_BATT_SHORT_V_THR_MASK) \
                                                >> ADP5061_BATT_SHORT_V_THR_OFFSET)
#define ADP5061_BATT_SHORT_V_THR_SET(a)     ((a & ((1 << ADP5061_BATT_SHORT_V_THR_LEN)-1)) \
                                                << ADP5061_BATT_SHORT_V_THR_OFFSET)
#define ADP5061_BATT_SHORT_V_THR_2V0        0x0
#define ADP5061_BATT_SHORT_V_THR_2V1        0x1
#define ADP5061_BATT_SHORT_V_THR_2V2        0x2
#define ADP5061_BATT_SHORT_V_THR_2V3        0x3
#define ADP5061_BATT_SHORT_V_THR_2V4        0x4
#define ADP5061_BATT_SHORT_V_THR_2V5        0x5
#define ADP5061_BATT_SHORT_V_THR_2V6        0x6
#define ADP5061_BATT_SHORT_V_THR_2V7        0x7

/* REG_IEND     0x11 */

/* IEND */
#define ADP5061_IEND_IEND_LEN           3
#define ADP5061_IEND_IEND_OFFSET        5
#define ADP5061_IEND_IEND_MASK          (((1 << ADP5061_IEND_IEND_LEN)-1) \
                                            << ADP5061_IEND_IEND_OFFSET)
#define ADP5061_IEND_IEND_GET(a)        ((a & ADP5061_IEND_IEND_MASK) \
                                            >> ADP5061_IEND_IEND_OFFSET)
#define ADP5061_IEND_IEND_SET(a)        ((a & ((1 << ADP5061_IEND_IEND_LEN)-1)) \
                                            << ADP5061_IEND_IEND_OFFSET)
#define ADP5061_IEND_IEND_12mA          0x0
#define ADP5061_IEND_IEND_32mA          0x1
#define ADP5061_IEND_IEND_52mA          0x2
#define ADP5061_IEND_IEND_72mA          0x3
#define ADP5061_IEND_IEND_92mA          0x4
#define ADP5061_IEND_IEND_117mA         0x5
#define ADP5061_IEND_IEND_142mA         0x6
#define ADP5061_IEND_IEND_170mA         0x7

/* C20_EOC */
#define ADP5061_C20_EOC_LEN             1
#define ADP5061_C20_EOC_OFFSET          4
#define ADP5061_C20_EOC_MASK            (((1 << ADP5061_C20_EOC_LEN)-1) \
                                            << ADP5061_C20_EOC_OFFSET)
#define ADP5061_C20_EOC_GET(a)          ((a & ADP5061_C20_EOC_MASK) \
                                            >> ADP5061_C20_EOC_OFFSET)
#define ADP5061_C20_EOC_SET(a)          ((a & ((1 << ADP5061_C20_EOC_LEN)-1)) \
                                            << ADP5061_C20_EOC_OFFSET)

/* C10_EOC */
#define ADP5061_C10_EOC_LEN             1
#define ADP5061_C10_EOC_OFFSET          3
#define ADP5061_C10_EOC_MASK            (((1 << ADP5061_C10_EOC_LEN)-1) \
                                            << ADP5061_C10_EOC_OFFSET)
#define ADP5061_C10_EOC_GET(a)          ((a & ADP5061_C10_EOC_MASK) \
                                            >> ADP5061_C10_EOC_OFFSET)
#define ADP5061_C10_EOC_SET(a)          ((a & ((1 << ADP5061_C10_EOC_LEN)-1)) \
                                            << ADP5061_C10_EOC_OFFSET)

/* C5_EOC */
#define ADP5061_C5_EOC_LEN              1
#define ADP5061_C5_EOC_OFFSET           2
#define ADP5061_C5_EOC_MASK             (((1 << ADP5061_C5_EOC_LEN)-1) \
                                            << ADP5061_C5_EOC_OFFSET)
#define ADP5061_C5_EOC_GET(a)           ((a & ADP5061_C5_EOC_MASK) \
                                            >> ADP5061_C5_EOC_OFFSET)
#define ADP5061_C5_EOC_SET(a)           ((a & ((1 << ADP5061_C5_EOC_LEN)-1)) \
                                            << ADP5061_C5_EOC_OFFSET)

/* SYS_EN_SET */
#define ADP5061_SYS_EN_LEN              2
#define ADP5061_SYS_EN_OFFSET           0
#define ADP5061_SYS_EN_MASK             (((1 << ADP5061_SYS_EN_LEN)-1) \
                                            << ADP5061_SYS_EN_OFFSET)
#define ADP5061_SYS_EN_GET(a)           ((a & ADP5061_SYS_EN_MASK) \
                                            >> ADP5061_SYS_EN_OFFSET)
#define ADP5061_SYS_EN_SET(a)           ((a & ((1 << ADP5061_SYS_EN_LEN)-1)) \
                                            << ADP5061_SYS_EN_OFFSET)

#endif /* _ADP5061_H */
