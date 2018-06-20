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

#ifndef __BQ27Z561_H__
#define __BQ27Z561_H__

#include "os/mynewt.h"

#ifdef __cplusplus
#extern "C" {
#endif

/* The i2c address of the device */
#define BQ27Z561_I2C_ADDR       (0x55)

/*
 * The maximum length of data allowed to write to any AltMfrgAccess command
 * XXX: no idea what the real size is yet.
 */
#define BQ27Z561_MAX_ALT_MFG_CMD_LEN    (32)

/* Maximum amount of bytes we will allow to read/write from flash in one call */
#define BQ27Z561_MAX_FLASH_RW_LEN       (64)

/* Flash address start/end */
#define BQ27Z561_FLASH_BEG_ADDR (0x4000)
#define BQ27Z561_FLASH_END_ADDR (0x4600)

/* Standard Data Commands */
#define BQ27Z561_REG_CNTL       (0x00)
#define BQ27Z561_REG_AR         (0x02)
#define BQ27Z561_REG_ARTTE      (0x04)
#define BQ27Z561_REG_TEMP       (0x06)
#define BQ27Z561_REG_VOLT       (0x08)
#define BQ27Z561_REG_FLAGS      (0x0A)
#define BQ27Z561_REG_INSTCURR   (0x0C)
#define BQ27Z561_REG_IMAX       (0x0E)
#define BQ27Z561_REG_RM         (0x10)
#define BQ27Z561_REG_FCC        (0x12)
#define BQ27Z561_REG_AI         (0x14)
#define BQ27Z561_REG_TTE        (0x16)
#define BQ27Z561_REG_TTF        (0x18)
#define BQ27Z561_REG_MLI        (0x1E)
#define BQ27Z561_REG_MLTTE      (0x20)
#define BQ27Z561_REG_AP         (0x22)
#define BQ27Z561_REG_INT_TEMP   (0x28)
#define BQ27Z561_REG_CC         (0x2A)
#define BQ27Z561_REG_RSOC       (0x2C)
#define BQ27Z561_REG_SOH        (0x2E)
#define BQ27Z561_REG_CV         (0x30)
#define BQ27Z561_REG_CHGC       (0x32)
#define BQ27Z561_REG_DCAP       (0x3C)
#define BQ27Z561_REG_MFRG_ACC   (0x3E)
#define BQ27Z561_REG_CHKSUM     (0x60)


/* Alt Manufacturer Command List */
#define BQ27Z561_CMD_DEV_TYPE               (0x0001)
#define BQ27Z561_CMD_FW_VER                 (0x0002)
#define BQ27Z561_CMD_HW_VER                 (0x0003)
#define BQ27Z561_CMD_IF_CHKSUM              (0x0004)
#define BQ27Z561_CMD_DF_SIG                 (0x0005)
#define BQ27Z561_CMD_CHEM_ID                (0x0006)
#define BQ27Z561_CMD_PREV_WR                (0x0007)
#define BQ27Z561_CMD_CHEM_DF_SIG            (0x0008)
#define BQ27Z561_CMD_ALL_DF_SIG             (0x0009)
#define BQ27Z561_CMD_RESET                  (0x0012)
#define BQ27Z561_CMD_GAUGING                (0x0021)
#define BQ27Z561_CMD_LIFETIME_DATA_COLLECT  (0x0023)
#define BQ27Z561_CMD_LIFETIME_DATA_RESET    (0x0028)
#define BQ27Z561_CMD_CALIBRATION_MODE       (0x002D)
#define BQ27Z561_CMD_LIFETIME_DATA_FLUSH    (0x002E)
#define BQ27Z561_CMD_SEAL_DEVICE            (0x0030)
#define BQ27Z561_CMD_SEC_KEYS               (0x0035)
#define BQ27Z561_CMD_RESET_DEV              (0x0041)
#define BQ27Z561_CMD_SET_DEEP_SLEEP         (0x0044)
#define BQ27Z561_CMD_CLR_DEEP_SLEEP         (0x0045)
#define BQ27Z561_CMD_PULSE_GPIO             (0x0046)
#define BQ27Z561_CMD_TAMBIENT_SYNC          (0x0047)
#define BQ27Z561_CMD_DEV_NAME               (0x004A)
#define BQ27Z561_CMD_DEV_CHEM               (0x004B)
#define BQ27Z561_CMD_MFG_NAME               (0x004C)
#define BQ27Z561_CMD_MFG_DATE               (0x004D)
#define BQ27Z561_CMD_SERIAL_NUM             (0x004E)
#define BQ27Z561_CMD_OP_STATUS              (0x0054)
#define BQ27Z561_CMD_CHG_STATUS             (0x0055)
#define BQ27Z561_CMD_GAUGING_STATUS         (0x0056)
#define BQ27Z561_CMD_MFG_STATUS             (0x0057)
#define BQ27Z561_CMD_LIFETIME_DATA_BLOCK1   (0x0060)
#define BQ27Z561_CMD_MFRG_DATA              (0x0070)
#define BQ27Z561_CMD_DA_STATUS1             (0x0071)
#define BQ27Z561_CMD_DA_STATUS2             (0x0072)
#define BQ27Z561_CMD_IT_STATUS1             (0x0073)
#define BQ27Z561_CMD_IT_STATUS2             (0x0074)
#define BQ27Z561_CMD_IT_STATUS3             (0x0075)
#define BQ27Z561_CMD_FCC_SOH                (0x0077)
#define BQ27Z561_CMD_FILT_CAP               (0x0078)
#define BQ27Z561_CMD_ROM_MODE               (0x0F00)
#define BQ27Z561_CMD_EXIT_CAL_MODE          (0xF080)
#define BQ27Z561_CMD_OUT_CC_ADC_CAL         (0xF081)
#define BQ27Z561_CMD_OUT_SHORT_CC_ADC_CAL   (0xF082)

/* Errors returned from some commands */
typedef enum
{
    BQ27Z561_OK = 0,
    BQ27Z561_ERR_CHKSUM_FAIL = 1,
    BQ27Z561_ERR_CMD_MISMATCH = 2,
    BQ27Z561_ERR_I2C_ERR = 3,
    BQ27Z561_ERR_CMD_LEN = 4,
    BQ27Z561_ERR_INV_PARAMS = 5,
    BQ27Z561_ERR_ALT_MFG_LEN = 6,
    BQ27Z561_ERR_INV_FLASH_ADDR = 7,
    BQ27Z561_ERR_FLASH_ADDR_MISMATCH = 8,
} bq27z561_err_t;

/* Config strucutre */
struct bq27z561_cfg
{
    /* XXX: not sure what config is as of yet */
    int foo;
};

/* Peripheral interface structure */
struct bq27z561_itf
{
    uint8_t itf_num;
    uint8_t itf_addr;
};

/* BQ27Z561 device */
struct bq27z561
{
    /* Underlying OS device */
    struct os_dev dev;

    /* Configuration values */
    struct bq27z561_cfg bq27_cfg;

    /* Interface */
    struct bq27z561_itf bq27_itf;
};

/**
 * bq27z561 set at rate
 *
 * Sets the value used in calculating the "at rate time to empty".
 *
 * @param dev pointer to device
 * @param current current rate, in mA
 *
 * @return int 0: success, -1 error
 */
int bq27z561_set_at_rate(struct bq27z561 *dev, int16_t current);

/**
 * bq27z561 get at rate
 *
 * Gets the value used in calculating the "at rate time to empty".
 *
 * @param dev pointer to device
 * @param current current rate, in mA
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_at_rate(struct bq27z561 *dev, int16_t *current);

/**
 * bq27z561 get time to empty
 *
 * Gets amount of time until the battery is fully discharged based on at rate.
 *
 * @param dev pointer to device
 * @param tte time until empty, in minutes
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_time_to_empty(struct bq27z561 *dev, uint16_t *tte);

/**
 * bq27z561 get temp
 *
 * Gets the temperature.
 *
 * @param dev pointer to device
 * @param temp_c temperature in deg C
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_temp(struct bq27z561 *dev, float *temp_c);

/**
 * bq27z561 get voltage
 *
 * Gets the measured cell voltage
 *
 * @param dev pointer to device
 * @param voltage voltage, in mV
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_voltage(struct bq27z561 *dev, uint16_t *voltage);

/**
 * bq27z561 get batt status
 *
 * Gets the battery status
 *
 * @param dev pointer to device
 * @param status battery status flags
 *      0x0010 FD Fully dischargd (0 no 1 yes)
 *      0x0020 FC Fully charged (0 no 1 yes)
 *      0x0040 DSG Discharging (0 charging 1 discharging)
 *      0x0080 INIT Initialization (0 complete 1 active)
 *      0x0200 RCA Remaining capacity alarm (0 inactive 1 active)
 *      0x0800 TDA Terminate discharge alarm (0 inactive 1 active)
 *      0x4000 TCA Terminate charge alarm (0 inactive 1 active)
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_batt_status(struct bq27z561 *dev, uint16_t *status);

/**
 * bq27z561 get current
 *
 * Gets the measured current from the coulomb counter
 *
 * @param dev pointer to device
 * @param current current, in mA
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_current(struct bq27z561 *dev, int16_t *current);

/**
 * bq27z561 get rem capacity
 *
 * Gets the predicted remaining capacity
 *
 * @param dev pointer to device
 * @param capacity, in mAH
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_rem_capacity(struct bq27z561 *dev, uint16_t *capacity);

/**
 * bq27z561 get full chg capacity
 *
 * Gets predicted full charge capacity
 *
 * @param dev pointer to device
 * @param capacity, in mAH
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_full_chg_capacity(struct bq27z561 *dev, uint16_t *capacity);

/**
 * bq27z561 get average current
 *
 * Gets average/filtered current
 *
 * @param dev pointer to device
 * @param current, in mA
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_avg_current(struct bq27z561 *dev, int16_t *current);

/**
 * bq27z561 get average time to empty
 *
 * Gets the predicted remaining battery capacity based on average current.
 *
 * @param dev pointer to device
 * @param tte average time to empty in minutes
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_avg_time_to_empty(struct bq27z561 *dev, uint16_t *tte);

/**
 * bq27z561 get average time to full
 *
 * Gets the predicted remaining time to full charge.
 *
 * @param dev pointer to device
 * @param ttf average time to full in minutes
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_avg_time_to_full(struct bq27z561 *dev, uint16_t *ttf);

/**
 * bq27z561 get average power
 *
 * Gets the average power (voltage * avg current). It is negative due to
 * discharge and positive due to charge. A zero value indicates battery not
 * being discharged.
 *
 * @param dev pointer to device
 * @param pwr average power in mW
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_avg_power(struct bq27z561 *dev, int16_t *pwr);

/**
 * bq27z561 get internal temp
 *
 * Gets the internal die temperature
 *
 * @param dev pointer to device
 * @param temp_c temperature, in degrees C.
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_internal_temp(struct bq27z561 *dev, float *temp_c);

/**
 * bq27z561 get cycle count
 *
 * Gets the number of discharge cycles
 *
 * @param dev pointer to device
 * @param cycles number of discharge cycles
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_discharge_cycles(struct bq27z561 *dev, uint16_t *cycles);

/**
 * bq27z561 get relatives state of charge
 *
 * Gets predicted remaining capacity as a percentage of full charge capacity
 *
 * @param dev pointer to device
 * @param pcnt
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_relative_state_of_charge(struct bq27z561 *dev, uint8_t *pcnt);

/**
 * bq27z561 get state of health
 *
 * Returns the state of health as a percentage of the design capacity
 *
 * @param dev pointer to device
 * @param pcnt % of design capacity
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_state_of_health(struct bq27z561 *dev, uint8_t *pcnt);

/**
 * bq27z561 get charging voltage
 *
 * Returns the desired charging voltage
 *
 * @param dev pointer to device
 * @param voltage desired charging current, in mV
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_charging_voltage(struct bq27z561 *dev, uint16_t *voltage);

/**
 * bq27z561 get charging current
 *
 * Returns the desired charging current
 *
 * @param dev pointer to device
 * @param current desired charging current, in mA
 *
 * @return int 0: success, -1 error
 */
int bq27z561_get_charging_current(struct bq27z561 *dev, uint16_t *current);

/**
 * bq27z561 read flash
 *
 * Reads 'buflen' bytes from flash at address 'addr'
 *
 * @param dev
 * @param addr Flash address to read from.
 * @param buf pointer to buffer where flash data will be stored. Cannot be NULL
 * @param buflen Number of bytes to read. Cannot be 0 and it must be less than
 *               or equal to BQ27Z561_MAX_FLASH_READ
 *
 * @return bq27z561_err_t
 */
bq27z561_err_t bq27x561_rd_flash(struct bq27z561 *dev, uint16_t addr,
                                 uint8_t *buf, int buflen);

bq27z561_err_t bq27x561_rd_alt_mfg_cmd(struct bq27z561 *dev, uint16_t cmd,
                                       uint8_t *val, int val_len);

/**
 * bq27z561 rd std reg word
 *
 * Allows reading of a standard "command" (register).
 *
 * NOTE: it is not expected that this api will be used by drivers or
 * applications. Its use is intended to be internal but is provided for
 * use by the shell.
 *
 * @param dev pointer to device
 * @param reg register to read
 * @param val returned value.
 *
 * @return int
 */
int bq27z561_rd_std_reg_word(struct bq27z561 *dev, uint8_t reg, uint16_t *val);


/**
 * Configure the bq27z561.
 *
 * @param ptr to bq27z561 device
 * @param ptr to bq27z561 config
 *
 * @return 0 on success, non-zero on failure.
 */
int bq27z561_config(struct bq27z561 * bq27z561, struct bq27z561_cfg * cfg);

/**
 * Expects to be called back through os_dev_create().
 *
 * @param ptr to the device object associated with this accelerometer
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int bq27z561_init(struct os_dev * dev, void * arg);

#if MYNEWT_VAL(BQ27Z561_CLI)
/**
 * Initialize the BQ27Z561 shell extensions.
 *
 * @return 0 on success, non-zero on failure.
 */
int bq27z561_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
