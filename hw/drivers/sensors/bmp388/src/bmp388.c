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
/**\mainpage
* Copyright (C) 2017 - 2019 Bosch Sensortec GmbH
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* Neither the name of the copyright holder nor the names of the
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER
* OR CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
* OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
*
* The information provided is believed to be accurate and reliable.
* The copyright holder assumes no responsibility
* for the consequences of use
* of such information nor for any infringement of patents or
* other rights of third parties which may result from its use.
* No license is granted by implication or otherwise under any patent or
* patent rights of the copyright holder.
*
* File   bmp388.c
* Date   10 May 2019
* Version   1.0.2
*
*/

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "os/mynewt.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#else
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"
#endif
#include "sensor/sensor.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"
#include "bmp388/bmp388.h"
#include "bmp388_priv.h"
#include "hal/hal_gpio.h"
#include "modlog/modlog.h"
#include "stats/stats.h"
#include <syscfg/syscfg.h>

#define COMPENSTATE_DEBUG 0
#define FIFOPARSE_DEBUG 0
#define CLEAR_INT_AFTER_ISR 0
#define BMP388_MAX_STREAM_MS 200000
#define BMP388_DEBUG 0
/*
* Max time to wait for interrupt.
*/
#define BMP388_MAX_INT_WAIT (10 * OS_TICKS_PER_SEC)

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
static struct hal_spi_settings spi_bmp388_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE0,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};
#endif

/* Define stat names for querying */
STATS_NAME_START(bmp388_stat_section)
    STATS_NAME(bmp388_stat_section, write_errors)
    STATS_NAME(bmp388_stat_section, read_errors)
STATS_NAME_END(bmp388_stat_section)

struct bmp3_dev g_bmp388_dev;


#define BMP388_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(BMP388_LOG_MODULE), __VA_ARGS__)

/* Exports for the sensor API */
static int bmp388_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int bmp388_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);
static int bmp388_sensor_set_notification(struct sensor *,
                                            sensor_event_type_t);
static int bmp388_sensor_unset_notification(struct sensor *,
                                            sensor_event_type_t);
static int bmp388_sensor_handle_interrupt(struct sensor *);
static int bmp388_sensor_set_config(struct sensor *, void *);

/*!
* @brief This internal API is used to validate the device pointer for
* null conditions.
*
* @param[in] dev : Structure instance of bmp3_dev.
*
* @return Result of API execution status
* @retval zero -> Success / +ve value -> Warning / -ve value -> Error
*/
static int8_t null_ptr_check(const struct bmp3_dev *dev);

/*!
* @brief This internal API sets the pressure enable and
* temperature enable settings of the sensor.
*
* @param[in] desired_settings : Contains the settings which user wants to
* change.
* @param[in] dev : Structure instance of bmp3_dev.
*
* @return Result of API execution status
* @retval zero -> Success / +ve value -> Warning / -ve value -> Error
*/
static int8_t set_pwr_ctrl_settings(struct sensor_itf *itf, uint32_t desired_settings, const struct bmp3_dev *dev);

/*!
* @brief This internal API sets the over sampling, odr and filter settings of
* the sensor based on the settings selected by the user.
*
* @param[in] desired_settings : Variable used to select the settings which
* are to be set.
* @param[in] dev : Structure instance of bmp3_dev.
*
* @return Result of API execution status
* @retval zero -> Success / +ve value -> Warning / -ve value -> Error
*/
static int8_t set_odr_filter_settings(struct sensor_itf *itf, uint32_t desired_settings, struct bmp3_dev *dev);


static const struct sensor_driver g_bmp388_sensor_driver = {
    .sd_read               = bmp388_sensor_read,
    .sd_set_config         = bmp388_sensor_set_config,
    .sd_get_config         = bmp388_sensor_get_config,
    .sd_set_notification   = bmp388_sensor_set_notification,
    .sd_unset_notification = bmp388_sensor_unset_notification,
    .sd_handle_interrupt   = bmp388_sensor_handle_interrupt

};

static void
delay_msec(uint32_t delay)
{
    delay = (delay * OS_TICKS_PER_SEC) / 1000 + 1;
    os_time_delay(delay);
}

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
* Write multiple length data to BMP388 sensor over I2C  (MAX: 19 bytes)
*
* @param The sensor interface
* @param register address
* @param variable length payload
* @param length of the payload to write
*
* @return BMP3_OK on success, non-zero on failure
*/
static int
bmp388_i2c_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *buffer,
                    uint8_t len)
{
    int rc;
    uint8_t payload[20] = { addr, 0, 0, 0, 0, 0, 0, 0,
                            0, 0, 0, 0, 0, 0, 0, 0,
                            0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = len + 1,
        .buffer = payload
    };

    if (len > (sizeof(payload) - 1)) {
        rc = BMP3_E_INVALID_LEN;
        goto err;
    }

    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = i2cn_master_write(itf->si_num, &data_struct, MYNEWT_VAL(BMP388_I2C_TIMEOUT_TICKS), 1,
                        MYNEWT_VAL(BMP388_I2C_RETRIES));
    if (rc) {
        BMP388_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                   data_struct.address);
        goto err;
    }

    return BMP3_OK;
err:
    return rc;
}

/**
* Write multiple length data to BMP388 sensor over SPI
*
* @param The sensor interface
* @param register address
* @param variable length payload
* @param length of the payload to write
*
* @return BMP3_OK on success, non-zero on failure
*/
static int
bmp388_spi_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                    uint16_t len)
{
    int i;
    int rc;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);


    /* Send the address */
    rc = hal_spi_tx_val(itf->si_num, addr);
    if (rc == 0xFFFF) {
        rc = BMP3_E_WRITE;
        BMP388_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                    itf->si_num, addr);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        rc = hal_spi_tx_val(itf->si_num, payload[i]);
        if (rc == 0xFFFF) {
            rc = BMP3_E_WRITE;
            BMP388_LOG(ERROR, "SPI_%u write failed addr:0x%02X:0x%02X\n",
                        itf->si_num, addr);
            goto err;
        }
    }


    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}
#endif

/**
* Write multiple length data to BMP388 sensor over different interfaces
*
* @param The sensor interface
* @param register address
* @param variable length payload
* @param length of the payload to write
*
* @return BMP3_OK on success, non-zero on failure
*/
int
bmp388_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                uint16_t len)
{
    int rc;
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct {
        uint8_t addr;
        /* max payload of 20 */
        uint8_t payload[19];
    } write_data;

    if (len > sizeof(write_data.payload)) {
        return -1;
    }

    write_data.addr = addr;
    memcpy(write_data.payload, payload, len);
    rc = bus_node_simple_write(itf->si_dev, &write_data, len + 1);

#else
    rc = sensor_itf_lock(itf, MYNEWT_VAL(BMP388_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = bmp388_i2c_writelen(itf, addr, payload, len);
    } else {
        rc = bmp388_spi_writelen(itf, addr, payload, len);
    }

    sensor_itf_unlock(itf);
#endif

    return rc;
}

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
* Read multiple bytes starting from specified register over i2c
*
* @param The sensor interface
* @param The register address start reading from
* @param Pointer to where the register value should be written
* @param Number of bytes to read
*
* @return 0 on success, non-zero error on failure.
*/
int
bmp388_i2c_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                    uint16_t len)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &reg
    };

    /* Register write */
    rc = i2cn_master_write(itf->si_num, &data_struct, MYNEWT_VAL(BMP388_I2C_TIMEOUT_TICKS), 1,
                        MYNEWT_VAL(BMP388_I2C_RETRIES));
    if (rc) {
        BMP388_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                    itf->si_addr);
        rc = BMP3_E_WRITE;
        return rc;
    }

    /* Read data */
    data_struct.len = len;
    data_struct.buffer = buffer;
    rc = i2cn_master_read(itf->si_num, &data_struct, MYNEWT_VAL(BMP388_I2C_TIMEOUT_TICKS), 1,
                        MYNEWT_VAL(BMP388_I2C_RETRIES));

    if (rc) {
        BMP388_LOG(ERROR, "Failed to read from 0x%02X:0x%02X\n",
                    itf->si_addr, reg);
        rc = BMP3_E_READ;
    }

    return rc;
}

/**
* Read multiple bytes starting from specified register over SPI
*
* @param The sensor interface
* @param The register address start reading from
* @param Pointer to where the register value should be written
* @param Number of bytes to read
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_spi_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                    uint16_t len)
{
    int i;
    uint16_t retval;
    int rc = 0;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, reg | BMP388_SPI_READ_CMD_BIT);

    if (retval == 0xFFFF) {
        BMP388_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                    itf->si_num, reg);
        rc = BMP3_E_READ;
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0);
        if (retval == 0xFFFF) {
            BMP388_LOG(ERROR, "SPI_%u read failed addr:0x%02X\n",
                        itf->si_num, reg);
            rc = BMP3_E_READ;
            goto err;
        }
        buffer[i] = retval;
    }

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}
#endif

/**
* Read multiple bytes starting from specified register over different interfaces
*
* @param The sensor interface
* @param The register address start reading from
* @param Pointer to where the register value should be written
* @param Number of bytes to read
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                uint16_t len)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bmp388 *dev = (struct bmp388 *)itf->si_dev;

    if (dev->node_is_spi) {
        reg |= BMP388_SPI_READ_CMD_BIT;
    }

    rc = bus_node_simple_write_read_transact(itf->si_dev, &reg, 1, buffer, len);

#else
    rc = sensor_itf_lock(itf, MYNEWT_VAL(BMP388_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = bmp388_i2c_readlen(itf, reg, buffer, len);
    } else {
        rc = bmp388_spi_readlen(itf, reg, buffer, len);
    }

    sensor_itf_unlock(itf);
#endif
    return rc;
}

/*!
* @brief This internal API is used to validate the device structure pointer for
* null conditions.
*/
static int8_t null_ptr_check(const struct bmp3_dev *dev)
{
    int8_t rslt;

    if ( dev == NULL) {
        /* Device structure pointer is not valid */
        rslt = BMP3_E_NULL_PTR;
    } else {
        /* Device structure is fine */
        rslt = BMP3_OK;
    }

    return rslt;
}

/*!
* @brief This internal API is used to identify the settings which the user
* wants to modify in the sensor.
*/
static uint8_t are_settings_changed(uint32_t sub_settings, uint32_t desired_settings)
{
    uint8_t settings_changed = FALSE;

    if (sub_settings & desired_settings) {
        /* User wants to modify this particular settings */
        settings_changed = TRUE;
    } else {
        /* User don't want to modify this particular settings */
        settings_changed = FALSE;
    }

    return settings_changed;
}

/*!
* @brief This internal API interleaves the register address between the
* register data buffer for burst write operation.
*/
static void interleave_reg_addr(const uint8_t *reg_addr, uint8_t *temp_buff,
                                const uint8_t *reg_data, uint8_t len)
{
    uint8_t index;
    /* conbime for bmp388 burst write */
    for (index = 1; index < len; index++) {
        temp_buff[(index * 2) - 1] = reg_addr[index];
        temp_buff[index * 2] = reg_data[index];
    }
}


/*!
* @brief This API reads the data from the given register address of the sensor.
*/
int8_t bmp3_get_regs(struct sensor_itf *itf, uint8_t reg_addr,
                     uint8_t *reg_data, uint16_t len,
                     const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint16_t temp_len = len + dev->dummy_byte;
    uint16_t i;
    uint8_t temp_buff[len + dev->dummy_byte];
    struct bmp388 *bmp388;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    /* Proceed if null check is fine */
    if (rslt ==  BMP3_OK) {
        rslt = bmp388_readlen(itf, reg_addr, temp_buff, temp_len);
        for (i = 0; i < len; i++)
            reg_data[i] = temp_buff[i + dev->dummy_byte];

        /* Check for communication error */
        if (rslt != BMP3_OK) {
            bmp388 = (struct bmp388 *)dev;
            rslt = BMP3_E_COMM_FAIL;
            if (rslt == BMP3_E_READ) {
               STATS_INC(bmp388->stats, read_errors);
            } else {
               STATS_INC(bmp388->stats, write_errors);
            }
        }
    }

    return rslt;
}

/*!
* @brief This API writes the given data to the register address
* of the sensor.
*/
int8_t bmp3_set_regs(struct sensor_itf *itf, uint8_t *reg_addr,
                     const uint8_t *reg_data, uint8_t len,
                     const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t temp_buff[len * 2];
    uint16_t temp_len;
    uint8_t reg_addr_cnt;
    struct bmp388 *bmp388;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    /* Check for arguments validity */
    if ((rslt == BMP3_OK) && (reg_addr != NULL) && (reg_data != NULL)) {
        if (len != 0) {
            temp_buff[0] = reg_data[0];
            /* If interface selected is SPI */
            if (dev->intf == BMP3_SPI_INTF) {
                for (reg_addr_cnt = 0; reg_addr_cnt < len; reg_addr_cnt++)
                    reg_addr[reg_addr_cnt] = reg_addr[reg_addr_cnt] & 0x7F;
            }
            /* Burst write mode */
            if (len > 1) {
                /* Interleave register address w.r.t data for
                burst write*/
                interleave_reg_addr(reg_addr, temp_buff, reg_data, len);
                temp_len = len * 2;
            } else {
                temp_len = len;
            }
            rslt = bmp388_writelen(itf, reg_addr[0], temp_buff, temp_len);
            /* Check for communication error */
            if (rslt != BMP3_OK) {
                bmp388 = (struct bmp388 *)dev;
                rslt = BMP3_E_COMM_FAIL;
                if (rslt == BMP3_E_READ) {
                    STATS_INC(bmp388->stats, read_errors);
                } else {
                    STATS_INC(bmp388->stats, write_errors);
                }
            }
        } else {
            rslt = BMP3_E_INVALID_LEN;
        }
    } else {
        rslt = BMP3_E_NULL_PTR;
    }

    return rslt;
}

/*!
* @brief This internal API fills the register address and register data of
* the over sampling settings for burst write operation.
*/
static void fill_osr_data(uint32_t settings, uint8_t *addr, uint8_t *reg_data, uint8_t *len,
                const struct bmp3_dev *dev)
{
    struct bmp3_odr_filter_settings osr_settings = dev->settings.odr_filter;

    if (settings & (BMP3_PRESS_OS_SEL | BMP3_TEMP_OS_SEL)) {
        /* Pressure over sampling settings check */
        if (settings & BMP3_PRESS_OS_SEL) {
            /* Set the pressure over sampling settings in the
            register variable */
            reg_data[*len] = BMP3_SET_BITS_POS_0(reg_data[0], BMP3_PRESS_OS, osr_settings.press_os);
        }
        /* Temperature over sampling settings check */
        if (settings & BMP3_TEMP_OS_SEL) {
            /* Set the temperature over sampling settings in the
            register variable */
            reg_data[*len] = BMP3_SET_BITS(reg_data[0], BMP3_TEMP_OS, osr_settings.temp_os);
        }
        /* 0x1C is the register address of over sampling register */
        addr[*len] = BMP3_OSR_ADDR;
        (*len)++;
    }
}

/*!
* @brief This internal API fills the register address and register data of
* the odr settings for burst write operation.
*/
static void fill_odr_data(uint8_t *addr, uint8_t *reg_data, uint8_t *len, struct bmp3_dev *dev)
{
    struct bmp3_odr_filter_settings *osr_settings = &dev->settings.odr_filter;

    /* Limit the ODR to 0.001525879 Hz*/
    if (osr_settings->odr > BMP3_ODR_0_001_HZ)
        osr_settings->odr = BMP3_ODR_0_001_HZ;
    /* Set the odr settings in the register variable */
    reg_data[*len] = BMP3_SET_BITS_POS_0(reg_data[1], BMP3_ODR, osr_settings->odr);
    /* 0x1D is the register address of output data rate register */
    addr[*len] = 0x1D;
    (*len)++;
}

/*!
* @brief This internal API fills the register address and register data of
* the filter settings for burst write operation.
*/
static void fill_filter_data(uint8_t *addr, uint8_t *reg_data, uint8_t *len, const struct bmp3_dev *dev)
{
    struct bmp3_odr_filter_settings osr_settings = dev->settings.odr_filter;

    /* Set the iir settings in the register variable */
    reg_data[*len] = BMP3_SET_BITS(reg_data[3], BMP3_IIR_FILTER, osr_settings.iir_filter);
    /* 0x1F is the register address of iir filter register */
    addr[*len] = 0x1F;
    (*len)++;
}

/*!
* @brief This internal API is used to calculate the power functionality.
*/
static uint32_t bmp3_pow(uint8_t base, uint8_t power)
{
    uint32_t pow_output = 1;

    while (power != 0) {
        pow_output = base * pow_output;
        power--;
    }

    return pow_output;
}

/*!
* @brief This internal API calculates the pressure measurement duration of the
* sensor.
*/
static uint16_t calculate_press_meas_time(const struct bmp3_dev *dev)
{
    uint16_t press_meas_t;
    struct bmp3_odr_filter_settings odr_filter = dev->settings.odr_filter;
#ifdef BMP3_DOUBLE_PRECISION_COMPENSATION
    double base = 2.0;
    double partial_out;
#else
    uint8_t base = 2;
    uint32_t partial_out;
#endif /* BMP3_DOUBLE_PRECISION_COMPENSATION */

    partial_out = bmp3_pow(base, odr_filter.press_os);
    press_meas_t = (uint16_t)(BMP3_PRESS_SETTLE_TIME + partial_out * BMP3_ADC_CONV_TIME);
    /* convert into mill seconds */
    press_meas_t = press_meas_t / 1000;

    return press_meas_t;
}

/*!
* @brief This internal API calculates the temperature measurement duration of
* the sensor.
*/
static uint16_t calculate_temp_meas_time(const struct bmp3_dev *dev)
{
    uint16_t temp_meas_t;
    struct bmp3_odr_filter_settings odr_filter = dev->settings.odr_filter;
#ifdef BMP3_DOUBLE_PRECISION_COMPENSATION
    float base = 2.0;
    float partial_out;
#else
    uint8_t base = 2;
    uint32_t partial_out;
#endif /* BMP3_DOUBLE_PRECISION_COMPENSATION */

    partial_out = bmp3_pow(base, odr_filter.temp_os);
    temp_meas_t = (uint16_t)(BMP3_TEMP_SETTLE_TIME + partial_out * BMP3_ADC_CONV_TIME);
    /* convert into mill seconds */
    temp_meas_t = temp_meas_t / 1000;

    return temp_meas_t;
}

/*!
* @brief This internal API checks whether the measurement time and odr duration
* of the sensor are proper.
*/
static int8_t verify_meas_time_and_odr_duration(uint16_t meas_t, uint32_t odr_duration)
{
    int8_t rslt;

    if (meas_t < odr_duration) {
        /* If measurement duration is less than odr duration
        then osr and odr settings are fine */
        rslt = BMP3_OK;
    } else {
        /* Osr and odr settings are not proper */
        rslt = BMP3_E_INVALID_ODR_OSR_SETTINGS;
    }

    return rslt;
}

/*!
* @brief This internal API validate the over sampling, odr settings of the
* sensor.
*/
static int8_t validate_osr_and_odr_settings(const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint16_t meas_t = 0;
    /* Odr values in milli secs  */
    uint32_t odr[18] = {5, 10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120, 10240,
            20480, 40960, 81920, 163840, 327680, 655360};

    if (dev->settings.press_en) {
        /* Calculate the pressure measurement duration */
        meas_t = calculate_press_meas_time(dev);
    }
    if (dev->settings.temp_en) {
        /* Calculate the temperature measurement duration */
        meas_t += calculate_temp_meas_time(dev);
    }
    rslt = verify_meas_time_and_odr_duration(meas_t, odr[dev->settings.odr_filter.odr]);

    return rslt;
}

/*!
* @brief This internal API parse the over sampling, odr and filter
* settings and store in the device structure.
*/
static void  parse_odr_filter_settings(const uint8_t *reg_data, struct bmp3_odr_filter_settings *settings)
{
    uint8_t index = 0;

    /* Odr and filter settings index starts from one (0x1C register) */
    settings->press_os = BMP3_GET_BITS_POS_0(reg_data[index], BMP3_PRESS_OS);
    settings->temp_os = BMP3_GET_BITS(reg_data[index], BMP3_TEMP_OS);

    /* Move index to 0x1D register */
    index++;
    settings->odr = BMP3_GET_BITS_POS_0(reg_data[index], BMP3_ODR);

    /* Move index to 0x1F register */
    index = index + 2;
    settings->iir_filter = BMP3_GET_BITS(reg_data[index], BMP3_IIR_FILTER);
}


/*!
* @brief This API sets the pressure enable and temperature enable
* settings of the sensor.
*/
static int8_t set_pwr_ctrl_settings(struct sensor_itf *itf, uint32_t desired_settings, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_addr = BMP3_PWR_CTRL_ADDR;
    uint8_t reg_data;

    rslt = bmp388_readlen(itf, reg_addr, &reg_data, 1);

    if (rslt == BMP3_OK) {
        if (desired_settings & BMP3_PRESS_EN_SEL) {
            /* Set the pressure enable settings in the
            register variable */
            reg_data = BMP3_SET_BITS_POS_0(reg_data, BMP3_PRESS_EN, dev->settings.press_en);
        }
        if (desired_settings & BMP3_TEMP_EN_SEL) {
            /* Set the temperature enable settings in the
            register variable */
            reg_data = BMP3_SET_BITS(reg_data, BMP3_TEMP_EN, dev->settings.temp_en);
        }
        /* Write the power control settings in the register */
        rslt = bmp388_writelen(itf, reg_addr, &reg_data, 1);
    }

    return rslt;
}

/*!
* @brief This internal API sets the over sampling, odr and filter settings
* of the sensor based on the settings selected by the user.
*/
static int8_t set_odr_filter_settings(struct sensor_itf *itf, uint32_t desired_settings, struct bmp3_dev *dev)
{
    int8_t rslt;
    /* No of registers to be configured is 3*/
    uint8_t reg_addr[3] = {0};
    /* No of register data to be read is 4 */
    uint8_t reg_data[4];
    uint8_t len = 0;

    rslt = bmp3_get_regs(itf, BMP3_OSR_ADDR, reg_data, 4, dev);

    if (rslt == BMP3_OK) {
        if (are_settings_changed((BMP3_PRESS_OS_SEL | BMP3_TEMP_OS_SEL), desired_settings)) {
            /* Fill the over sampling register address and
            register data to be written in the sensor */
            fill_osr_data(desired_settings, reg_addr, reg_data, &len, dev);
        }
        if (are_settings_changed(BMP3_ODR_SEL, desired_settings)) {
            /* Fill the output data rate register address and
            register data to be written in the sensor */
            fill_odr_data(reg_addr, reg_data, &len, dev);
        }
        if (are_settings_changed(BMP3_IIR_FILTER_SEL, desired_settings)) {
            /* Fill the iir filter register address and
            register data to be written in the sensor */
            fill_filter_data(reg_addr, reg_data, &len, dev);
        }
        if (dev->settings.op_mode == BMP3_NORMAL_MODE) {
            /* For normal mode, osr and odr settings should
            be proper */
            rslt = validate_osr_and_odr_settings(dev);
        }
        if (rslt == BMP3_OK) {
            /* Burst write the over sampling, odr and filter
            settings in the register */
            rslt = bmp3_set_regs(itf, reg_addr, reg_data, len, dev);
        }
    }

    return rslt;
}

/*!
* @brief This internal API sets the interrupt control (output mode, level,
* latch and data ready) settings of the sensor based on the settings
* selected by the user.
*/
static int8_t set_int_ctrl_settings(struct sensor_itf *itf, uint32_t desired_settings, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;
    uint8_t reg_addr;
    struct bmp3_int_ctrl_settings int_settings;

    reg_addr = BMP3_INT_CTRL_ADDR;
    rslt = bmp3_get_regs(itf, reg_addr, &reg_data, 1, dev);

    if (rslt == BMP3_OK) {
        int_settings = dev->settings.int_settings;

        if (desired_settings & BMP3_OUTPUT_MODE_SEL) {
            /* Set the interrupt output mode bits */
            reg_data = BMP3_SET_BITS_POS_0(reg_data, BMP3_INT_OUTPUT_MODE, int_settings.output_mode);
        }
        if (desired_settings & BMP3_LEVEL_SEL) {
            /* Set the interrupt level bits */
            reg_data = BMP3_SET_BITS(reg_data, BMP3_INT_LEVEL, int_settings.level);
        }
        if (desired_settings & BMP3_LATCH_SEL) {
            /* Set the interrupt latch bits */
            reg_data = BMP3_SET_BITS(reg_data, BMP3_INT_LATCH, int_settings.latch);
        }
        if (desired_settings & BMP3_DRDY_EN_SEL) {
            /* Set the interrupt data ready bits */
            reg_data = BMP3_SET_BITS(reg_data, BMP3_INT_DRDY_EN, int_settings.drdy_en);
        }

        rslt = bmp3_set_regs(itf, &reg_addr, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
* @brief This internal API sets the advance (i2c_wdt_en, i2c_wdt_sel)
* settings of the sensor based on the settings selected by the user.
*/
static int8_t set_advance_settings(struct sensor_itf *itf, uint32_t desired_settings, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_addr;
    uint8_t reg_data;
    struct bmp3_adv_settings adv_settings = dev->settings.adv_settings;

    reg_addr = BMP3_IF_CONF_ADDR;
    rslt = bmp3_get_regs(itf, reg_addr, &reg_data, 1, dev);

    if (rslt == BMP3_OK) {
        if (desired_settings & BMP3_I2C_WDT_EN_SEL) {
            /* Set the i2c watch dog enable bits */
            reg_data = BMP3_SET_BITS(reg_data, BMP3_I2C_WDT_EN, adv_settings.i2c_wdt_en);
        }
        if (desired_settings & BMP3_I2C_WDT_SEL_SEL) {
            /* Set the i2c watch dog select bits */
            reg_data = BMP3_SET_BITS(reg_data, BMP3_I2C_WDT_SEL, adv_settings.i2c_wdt_sel);
        }

        rslt = bmp3_set_regs(itf, &reg_addr, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
* @brief This API gets the power mode of the sensor.
*/
int8_t bmp3_get_op_mode(struct sensor_itf *itf, uint8_t *op_mode, const struct bmp3_dev *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);

    if (rslt == BMP3_OK) {
        /* Read the power mode register */
        rslt = bmp3_get_regs(itf, BMP3_PWR_CTRL_ADDR, op_mode, 1, dev);
        /* Assign the power mode in the device structure */
        *op_mode = BMP3_GET_BITS(*op_mode, BMP3_OP_MODE);
    }

    return rslt;
}

/*!
* @brief This internal API gets the over sampling, odr and filter settings
* of the sensor.
*/
static int8_t get_odr_filter_settings(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data[4];

    /* Read data beginning from 0x1C register */
    rslt = bmp3_get_regs(itf, BMP3_OSR_ADDR, reg_data, 4, dev);
    /* Parse the read data and store it in dev structure */
    parse_odr_filter_settings(reg_data, &dev->settings.odr_filter);

    return rslt;
}

/*!
* @brief This internal API puts the device to sleep mode.
*/
static int8_t put_device_to_sleep(struct sensor_itf *itf, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_addr = BMP3_PWR_CTRL_ADDR;
    /* Temporary variable to store the value read from op-mode register */
    uint8_t op_mode_reg_val;

    rslt = bmp3_get_regs(itf, BMP3_PWR_CTRL_ADDR, &op_mode_reg_val, 1, dev);

    if (rslt == BMP3_OK) {
        /* Set the power mode */
        op_mode_reg_val = op_mode_reg_val & (~(BMP3_OP_MODE_MSK));

        /* Write the power mode in the register */
        rslt = bmp3_set_regs(itf, &reg_addr, &op_mode_reg_val, 1, dev);
    }

    return rslt;
}

/*!
* @brief This internal API validate the normal mode settings of the sensor.
*/
static int8_t validate_normal_mode_settings(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;

    rslt = get_odr_filter_settings(itf, dev);

    if (rslt == BMP3_OK)
        rslt = validate_osr_and_odr_settings(dev);

    return rslt;
}

/*!
* @brief This internal API writes the power mode in the sensor.
*/
static int8_t write_power_mode(struct sensor_itf *itf, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_addr = BMP3_PWR_CTRL_ADDR;
    uint8_t op_mode = dev->settings.op_mode;
    /* Temporary variable to store the value read from op-mode register */
    uint8_t op_mode_reg_val;

    /* Read the power mode register */
    rslt = bmp3_get_regs(itf, reg_addr, &op_mode_reg_val, 1, dev);
    /* Set the power mode */
    if (rslt == BMP3_OK) {
        op_mode_reg_val = BMP3_SET_BITS(op_mode_reg_val, BMP3_OP_MODE, op_mode);
        /* Write the power mode in the register */
        rslt = bmp3_set_regs(itf, &reg_addr, &op_mode_reg_val, 1, dev);
    }

    return rslt;
}

/*!
* @brief This internal API sets the normal mode in the sensor.
*/
static int8_t set_normal_mode(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t conf_err_status;

    rslt = validate_normal_mode_settings(itf, dev);
    /* If osr and odr settings are proper then write the power mode */
    if (rslt == BMP3_OK) {
        rslt = write_power_mode(itf, dev);
        /* check for configuration error */
        if (rslt == BMP3_OK) {
            /* Read the configuration error status */
            rslt = bmp3_get_regs(itf, BMP3_ERR_REG_ADDR, &conf_err_status, 1, dev);
            /* Check if conf. error flag is set */
            if (rslt == BMP3_OK) {
                if (conf_err_status & BMP3_CONF_ERR) {
                    /* Osr and odr configuration is
                    not proper */
                    rslt = BMP3_E_CONFIGURATION_ERR;
                }
            }
        }
    }

    return rslt;
}

/*!
* @brief This API sets the power mode of the sensor.
*/
int8_t bmp3_set_op_mode(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t last_set_mode;
    uint8_t curr_mode = dev->settings.op_mode;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);

    if (rslt == BMP3_OK) {
        rslt = bmp3_get_op_mode(itf, &last_set_mode, dev);
        /* If the sensor is not in sleep mode put the device to sleep
        mode */
        if (last_set_mode != BMP3_SLEEP_MODE) {
            /* Device should be put to sleep before transiting to
            forced mode or normal mode */
            rslt = put_device_to_sleep(itf, dev);
            /* Give some time for device to go into sleep mode */
            delay_msec(5);
        }
        /* Set the power mode */
        if (rslt == BMP3_OK) {
            if (curr_mode == BMP3_NORMAL_MODE) {
                /* Set normal mode and validate
                necessary settings */
                rslt = set_normal_mode(itf, dev);
            } else if (curr_mode == BMP3_FORCED_MODE) {
                /* Set forced mode */
                rslt = write_power_mode(itf, dev);
            }
            /* Give some time for device to change mode */
            delay_msec(5);
        }
    }

    return rslt;
}

/*!
* @brief This API sets the power control(pressure enable and
* temperature enable), over sampling, odr and filter
* settings in the sensor.
*/
int8_t bmp3_set_sensor_settings(struct sensor_itf *itf, uint32_t desired_settings, struct bmp3_dev *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    /* Proceed if null check is fine */
    if (rslt ==  BMP3_OK) {
        if (are_settings_changed(POWER_CNTL, desired_settings)) {
            /* Set the power control settings */
            rslt = set_pwr_ctrl_settings(itf, desired_settings, dev);
        }
        if (are_settings_changed(ODR_FILTER, desired_settings) && (!rslt)) {
            /* Set the over sampling, odr and filter settings*/
            rslt = set_odr_filter_settings(itf, desired_settings, dev);
        }
        if (are_settings_changed(INT_CTRL, desired_settings) && (!rslt)) {
            /* Set the interrupt control settings */
            rslt = set_int_ctrl_settings(itf, desired_settings, dev);
        }
        if (are_settings_changed(ADV_SETT, desired_settings) && (!rslt)) {
            /* Set the advance settings */
            rslt = set_advance_settings(itf, desired_settings, dev);
        }
    }

    return rslt;
}

/**
* Set bmp388 to normal mode
*
* @param itf The sensor interface
*
* @return 0 on success, non-zero on failure
*/
int8_t bmp388_set_normal_mode(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;
    /* Select the pressure and temperature sensor to be enabled */
    dev->settings.press_en = BMP3_ENABLE;
    dev->settings.temp_en = BMP3_ENABLE;
    /* Select the output data rate and oversampling settings for pressure and temperature */
    dev->settings.odr_filter.press_os = BMP3_NO_OVERSAMPLING;
    dev->settings.odr_filter.temp_os = BMP3_NO_OVERSAMPLING;
    //dev->settings.odr_filter.odr = BMP3_ODR_200_HZ;
    dev->settings.odr_filter.odr = g_bmp388_dev.settings.odr_filter.odr;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_PRESS_OS_SEL | BMP3_TEMP_OS_SEL | BMP3_ODR_SEL;
    rslt = bmp3_set_sensor_settings(itf, settings_sel, dev);

    /* Set the power mode to normal mode */
    dev->settings.op_mode = BMP3_NORMAL_MODE;
    rslt = bmp3_set_op_mode(itf, dev);

    return rslt;


}

int8_t bmp388_set_forced_mode_with_osr(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;
    /* Select the pressure and temperature sensor to be enabled */
    dev->settings.press_en = BMP3_ENABLE;
    dev->settings.temp_en = BMP3_ENABLE;
    /* Select the oversampling settings for pressure and temperature */
    dev->settings.odr_filter.press_os = g_bmp388_dev.settings.odr_filter.press_os;
    dev->settings.odr_filter.temp_os = g_bmp388_dev.settings.odr_filter.temp_os;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_PRESS_OS_SEL | BMP3_TEMP_OS_SEL;
    /* Write the settings in the sensor */
    rslt = bmp3_set_sensor_settings(itf, settings_sel, dev);
    /* Select the power mode */
    dev->settings.op_mode = BMP3_FORCED_MODE;
    /* Set the power mode in the sensor */
    rslt = bmp3_set_op_mode(itf, dev);
    return rslt;
}

/*!
*  @brief This internal API is used to parse the pressure or temperature or
*  both the data and store it in the bmp3_uncomp_data structure instance.
*/
static void parse_sensor_data(const uint8_t *reg_data, struct bmp3_uncomp_data *uncomp_data)
{
    /* Temporary variables to store the sensor data */
    uint32_t data_xlsb;
    uint32_t data_lsb;
    uint32_t data_msb;

    /* Store the parsed register values for pressure data */
    data_xlsb = (uint32_t)reg_data[0];
    data_lsb = (uint32_t)reg_data[1] << 8;
    data_msb = (uint32_t)reg_data[2] << 16;
    uncomp_data->pressure = data_msb | data_lsb | data_xlsb;

    /* Store the parsed register values for temperature data */
    data_xlsb = (uint32_t)reg_data[3];
    data_lsb = (uint32_t)reg_data[4] << 8;
    data_msb = (uint32_t)reg_data[5] << 16;
    uncomp_data->temperature = data_msb | data_lsb | data_xlsb;
}

/*!
* @brief This internal API is used to compensate the raw temperature data and
* return the compensated temperature data in integer data type.
*/
static int64_t compensate_temperature(const struct bmp3_uncomp_data *uncomp_data,
                        struct bmp3_calib_data *calib_data)
{
    uint64_t partial_data1;
    uint64_t partial_data2;
    uint64_t partial_data3;
    int64_t partial_data4;
    int64_t partial_data5;
    int64_t partial_data6;
    int64_t comp_temp;
#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****uncomp_data->temperature = 0x%x calib_data->reg_calib_data.par_t1 = 0x%x\n", uncomp_data->temperature, calib_data->reg_calib_data.par_t1);
#endif
    partial_data1 = (uint64_t)(uncomp_data->temperature) - (256 * (uint64_t)(calib_data->reg_calib_data.par_t1));

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data1 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data1)>>32),(uint32_t)(partial_data1&0xffffffff));
    BMP388_LOG(ERROR, "*****calib_data->reg_calib_data.par_t2 = 0x%x\n", calib_data->reg_calib_data.par_t2);
#endif

    partial_data2 = ((uint64_t)(calib_data->reg_calib_data.par_t2)) * partial_data1;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data2 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data2)>>32),(uint32_t)((partial_data2&0xffffffff)));
#endif
    partial_data3 = partial_data1 * partial_data1;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data3 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data3)>>32),(uint32_t)((partial_data3&0xffffffff)));
    BMP388_LOG(ERROR, "*****calib_data->reg_calib_data.par_t3 = 0x%x\n", calib_data->reg_calib_data.par_t3);
#endif

    partial_data4 = (int64_t)partial_data3 * calib_data->reg_calib_data.par_t3;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data4 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data4)>>32),(uint32_t)((partial_data4&0xffffffff)));
#endif

    partial_data5 = ((int64_t)(partial_data2 * 262144) + partial_data4);

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data5 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data5)>>32),(uint32_t)(partial_data5&0xffffffff));
#endif

    partial_data6 = partial_data5 / 4294967296;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data6 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data6)>>32),(uint32_t)(partial_data6&0xffffffff));
#endif
    /* Store t_lin in dev. structure for pressure calculation */
    calib_data->reg_calib_data.t_lin = partial_data6;
    comp_temp = (int64_t)((partial_data6 * 25)  / 16384);

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****comp_temp high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((comp_temp)>>32),(uint32_t)(comp_temp&0xffffffff));
#endif

    return comp_temp;
}

/*!
* @brief This internal API is used to compensate the raw pressure data and
* return the compensated pressure data in integer data type.
*/
static uint64_t compensate_pressure(const struct bmp3_uncomp_data *uncomp_data,
                        const struct bmp3_calib_data *calib_data)
{
    const struct bmp3_reg_calib_data *reg_calib_data = &calib_data->reg_calib_data;
    int64_t partial_data1;
    int64_t partial_data2;
    int64_t partial_data3;
    int64_t partial_data4;
    int64_t partial_data5;
    int64_t partial_data6;
    int64_t offset;
    int64_t sensitivity;
    uint64_t comp_press;
#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****reg_calib_data->t_lin high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((reg_calib_data->t_lin)>>32),(uint32_t)(reg_calib_data->t_lin&0xffffffff));
#endif

    partial_data1 = reg_calib_data->t_lin * reg_calib_data->t_lin;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data1 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data1)>>32),(uint32_t)(partial_data1&0xffffffff));
#endif

    partial_data2 = partial_data1 / 64;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data2 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)(partial_data2>>32),(uint32_t)(partial_data2&0xffffffff));
#endif

    partial_data3 = (partial_data2 * reg_calib_data->t_lin) / 256;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data3 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data3)>>32),(uint32_t)(partial_data3&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p8 = %d\n", reg_calib_data->par_p8);
#endif

    partial_data4 = (reg_calib_data->par_p8 * partial_data3) / 32;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data4 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data4)>>32),(uint32_t)(partial_data4&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p7 = %d\n", reg_calib_data->par_p7);
#endif

    partial_data5 = (reg_calib_data->par_p7 * partial_data1) * 16;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data5 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data5)>>32),(uint32_t)(partial_data5&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p6 = %d\n", reg_calib_data->par_p6);
#endif

    partial_data6 = (reg_calib_data->par_p6 * reg_calib_data->t_lin) * 4194304;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data6 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data6)>>32),(uint32_t)(partial_data6&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p5 = %d\n", reg_calib_data->par_p5);
#endif

    offset = (reg_calib_data->par_p5 * 140737488355328) + partial_data4 + partial_data5 + partial_data6;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****offset high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((offset)>>32),(uint32_t)(offset&0xffffffff));

    BMP388_LOG(ERROR, "*****reg_calib_data->par_p4 = %d\n", reg_calib_data->par_p4);
#endif

    partial_data2 = (reg_calib_data->par_p4 * partial_data3) / 32;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data2 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data2)>>32),(uint32_t)(partial_data2&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p3 = %d\n", reg_calib_data->par_p3);
#endif

    partial_data4 = (reg_calib_data->par_p3 * partial_data1) * 4;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data4 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data4)>>32),(uint32_t)(partial_data4&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p2 = %d\n", reg_calib_data->par_p2);
#endif

    partial_data5 = (reg_calib_data->par_p2 - 16384) * reg_calib_data->t_lin * 2097152;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data5 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data5)>>32),(uint32_t)(partial_data5&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p1 = %d\n", reg_calib_data->par_p1);
#endif

    sensitivity = ((reg_calib_data->par_p1 - 16384) * 70368744177664) + partial_data2 + partial_data4
            + partial_data5;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****sensitivity high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((sensitivity)>>32),(uint32_t)(sensitivity&0xffffffff));
#endif

    partial_data1 = (sensitivity / 16777216) * uncomp_data->pressure;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data1 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data1)>>32),(uint32_t)(partial_data1&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p10 = %d\n", reg_calib_data->par_p10);
#endif

    partial_data2 = reg_calib_data->par_p10 * reg_calib_data->t_lin;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data2 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data2)>>32),(uint32_t)(partial_data2&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p9 = %d\n", reg_calib_data->par_p9);
#endif

    partial_data3 = partial_data2 + (65536 * reg_calib_data->par_p9);

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data3 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data3)>>32),(uint32_t)(partial_data3&0xffffffff));
#endif

    partial_data4 = (partial_data3 * uncomp_data->pressure) / 8192;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data4 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data4)>>32),(uint32_t)(partial_data4&0xffffffff));
#endif

    partial_data5 = (partial_data4 * uncomp_data->pressure) / 512;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data5 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data5)>>32),(uint32_t)(partial_data5&0xffffffff));
#endif

    partial_data6 = (int64_t)((uint64_t)uncomp_data->pressure * (uint64_t)uncomp_data->pressure);

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data6 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data6)>>32),(uint32_t)(partial_data6&0xffffffff));
    BMP388_LOG(ERROR, "*****reg_calib_data->par_p11 = %d\n", reg_calib_data->par_p11);
#endif

    partial_data2 = (reg_calib_data->par_p11 * partial_data6) / 65536;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data2 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data2)>>32),(uint32_t)(partial_data2&0xffffffff));
#endif

    partial_data3 = (partial_data2 * uncomp_data->pressure) / 128;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data3 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data3)>>32),(uint32_t)(partial_data3&0xffffffff));
#endif

    partial_data4 = (offset / 4) + partial_data1 + partial_data5 + partial_data3;

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****partial_data4 high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((partial_data4)>>32),(uint32_t)(partial_data4&0xffffffff));
#endif

    comp_press = (((uint64_t)partial_data4 * 25) / (uint64_t)1099511627776);

#if COMPENSTATE_DEBUG
    BMP388_LOG(ERROR, "*****comp_press high32bit = 0x%x low32bit = 0x%x\n", (uint32_t)((comp_press)>>32),(uint32_t)(comp_press&0xffffffff));
#endif

    return comp_press;
}

/*!
* @brief This internal API is used to compensate the pressure or temperature
* or both the data according to the component selected by the user.
*/
static int8_t compensate_data(uint8_t sensor_comp, const struct bmp3_uncomp_data *uncomp_data,
                    struct bmp3_data *comp_data, struct bmp3_calib_data *calib_data)
{
    int8_t rslt = BMP3_OK;

    if ((uncomp_data != NULL) && (comp_data != NULL) && (calib_data != NULL)) {
        /* If pressure or temperature component is selected */
        if (sensor_comp & (BMP3_PRESS | BMP3_TEMP)) {
            /* Compensate the temperature data */
            comp_data->temperature =compensate_temperature(uncomp_data, calib_data);
        }
        if (sensor_comp & BMP3_PRESS) {
            /* Compensate the pressure data */
            comp_data->pressure = compensate_pressure(uncomp_data, calib_data);
        }
    } else {
        rslt = BMP3_E_NULL_PTR;
    }

    return rslt;
}

/*!
* @brief This API reads the pressure, temperature or both data from the
* sensor, compensates the data and store it in the bmp3_data structure
* instance passed by the user.
*/
int8_t bmp3_get_sensor_data(struct sensor_itf *itf, uint8_t sensor_comp, struct bmp3_data *comp_data, struct bmp3_dev *dev)
{
    int8_t rslt;
    /* Array to store the pressure and temperature data read from
    the sensor */
    uint8_t reg_data[BMP3_P_T_DATA_LEN] = {0};
    struct bmp3_uncomp_data uncomp_data = {0};

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);

    if ((rslt == BMP3_OK) && (comp_data != NULL)) {
        /* Read the pressure and temperature data from the sensor */
        rslt = bmp3_get_regs(itf, BMP3_DATA_ADDR, reg_data, BMP3_P_T_DATA_LEN, dev);

        if (rslt == BMP3_OK) {
            /* Parse the read data from the sensor */
            parse_sensor_data(reg_data, &uncomp_data);
#if COMPENSTATE_DEBUG
            BMP388_LOG(ERROR, "*****uncomp_data Temperature = %d Pressure = %d\n", uncomp_data.temperature, uncomp_data.pressure);
#endif
            /* Compensate the pressure/temperature/both data read
            from the sensor */
            rslt = compensate_data(sensor_comp, &uncomp_data, comp_data, &dev->calib_data);
        }
    } else {
        rslt = BMP3_E_NULL_PTR;
    }

    return rslt;
}

int8_t bmp388_get_sensor_data(struct sensor_itf *itf, struct bmp3_dev *dev, struct bmp3_data *sensor_data)
{
    int8_t rslt;
    /* Variable used to select the sensor component */
    uint8_t sensor_comp;
    /* Variable used to store the compensated data */
    struct bmp3_data data;

    /* Sensor component selection */
    sensor_comp = BMP3_PRESS | BMP3_TEMP;
    /* Temperature and Pressure data are read and stored in the bmp3_data instance */
    rslt = bmp3_get_sensor_data(itf, sensor_comp, &data, dev);
    /* Print the temperature and pressure data */
    sensor_data->pressure = data.pressure;
    sensor_data->temperature = data.temperature;
    return rslt;
}

/*!
* @brief This API performs the soft reset of the sensor.
*/
int8_t bmp3_soft_reset(struct sensor_itf *itf, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_addr = BMP3_CMD_ADDR;
    /* 0xB6 is the soft reset command */
    uint8_t soft_rst_cmd = 0xB6;
    uint8_t cmd_rdy_status;
    uint8_t cmd_err_status;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    /* Proceed if null check is fine */
    if (rslt == BMP3_OK) {
        /* Check for command ready status */
        rslt = bmp3_get_regs(itf, BMP3_SENS_STATUS_REG_ADDR, &cmd_rdy_status, 1, dev);
        /* Device is ready to accept new command */
        if ((cmd_rdy_status & BMP3_CMD_RDY) && (rslt == BMP3_OK)) {
            /* Write the soft reset command in the sensor */
            rslt = bmp3_set_regs(itf, &reg_addr, &soft_rst_cmd, 1, dev);
            /* Proceed if everything is fine until now */
            if (rslt == BMP3_OK) {
                /* Wait for 2 ms */
                delay_msec(2);
                /* Read for command error status */
                rslt = bmp3_get_regs(itf, BMP3_ERR_REG_ADDR, &cmd_err_status, 1, dev);
                /* check for command error status */
                if ((cmd_err_status & BMP3_CMD_ERR) || (rslt != BMP3_OK)) {
                    /* Command not written hence return
                    error */
                    rslt = BMP3_E_CMD_EXEC_FAILED;
                }
            }
        } else {
            rslt = BMP3_E_CMD_EXEC_FAILED;
        }
    }

    return rslt;
}

/*!
* @brief This API performs the soft reset of the sensor.
*/
int8_t bmp3_fifo_flush(struct sensor_itf *itf, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_addr = BMP3_CMD_ADDR;
    /* 0xB6 is the soft reset command */
    uint8_t flush_rst_cmd = 0xB0;
    uint8_t cmd_rdy_status;
    uint8_t cmd_err_status;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    /* Proceed if null check is fine */
    if (rslt == BMP3_OK) {
        /* Check for command ready status */
        rslt = bmp3_get_regs(itf, BMP3_SENS_STATUS_REG_ADDR, &cmd_rdy_status, 1, dev);
        /* Device is ready to accept new command */
        if ((cmd_rdy_status & BMP3_CMD_RDY) && (rslt == BMP3_OK)) {
            /* Write the soft reset command in the sensor */
            rslt = bmp3_set_regs(itf, &reg_addr, &flush_rst_cmd, 1, dev);
            /* Proceed if everything is fine until now */
            if (rslt == BMP3_OK) {
                /* Wait for 2 ms */
                delay_msec(2);
                /* Read for command error status */
                rslt = bmp3_get_regs(itf, BMP3_ERR_REG_ADDR, &cmd_err_status, 1, dev);
                /* check for command error status */
                if ((cmd_err_status & BMP3_CMD_ERR) || (rslt != BMP3_OK)) {
                    /* Command not written hence return
                    error */
                    rslt = BMP3_E_CMD_EXEC_FAILED;
                }
            }
        } else {
            rslt = BMP3_E_CMD_EXEC_FAILED;
        }
    }

    return rslt;
}

/*!
*  @brief This internal API is used to parse the calibration data, compensates
*  it and store it in device structure
*/
static void parse_calib_data(const uint8_t *reg_data, struct bmp3_dev *dev)
{
    /* Temporary variable to store the aligned trim data */
    struct bmp3_reg_calib_data *reg_calib_data = &dev->calib_data.reg_calib_data;

    reg_calib_data->par_t1 = BMP3_CONCAT_BYTES(reg_data[1], reg_data[0]);
    reg_calib_data->par_t2 = BMP3_CONCAT_BYTES(reg_data[3], reg_data[2]);
    reg_calib_data->par_t3 = (int8_t)reg_data[4];
    reg_calib_data->par_p1 = (int16_t)BMP3_CONCAT_BYTES(reg_data[6], reg_data[5]);
    reg_calib_data->par_p2 = (int16_t)BMP3_CONCAT_BYTES(reg_data[8], reg_data[7]);
    reg_calib_data->par_p3 = (int8_t)reg_data[9];
    reg_calib_data->par_p4 = (int8_t)reg_data[10];
    reg_calib_data->par_p5 = BMP3_CONCAT_BYTES(reg_data[12], reg_data[11]);
    reg_calib_data->par_p6 = BMP3_CONCAT_BYTES(reg_data[14],  reg_data[13]);
    reg_calib_data->par_p7 = (int8_t)reg_data[15];
    reg_calib_data->par_p8 = (int8_t)reg_data[16];
    reg_calib_data->par_p9 = (int16_t)BMP3_CONCAT_BYTES(reg_data[18], reg_data[17]);
    reg_calib_data->par_p10 = (int8_t)reg_data[19];
    reg_calib_data->par_p11 = (int8_t)reg_data[20];
}

/*!
* @brief This internal API reads the calibration data from the sensor, parse
* it then compensates it and store in the device structure.
*/
static int8_t get_calib_data(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_addr = BMP3_CALIB_DATA_ADDR;
    /* Array to store calibration data */
    uint8_t calib_data[BMP3_CALIB_DATA_LEN] = {0};
    uint8_t index = 0;

    /* Read the calibration data from the sensor */
    //rslt = bmp3_get_regs(itf, reg_addr, calib_data, BMP3_CALIB_DATA_LEN, dev);
    for (index = 0; index < BMP3_CALIB_DATA_LEN; index++)
        rslt = bmp3_get_regs(itf, reg_addr + index, calib_data + index, 1, dev);
    /* Parse calibration data and store it in device structure */
    parse_calib_data(calib_data, dev);

    return rslt;
}

/*!
*  @brief This API is the entry point.
*  It performs the selection of I2C/SPI read mechanism according to the
*  selected interface and reads the chip-id and calibration data of the sensor.
*/
int8_t bmp3_init(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t chip_id = 0;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    /* Proceed if null check is fine */
    if (rslt ==  BMP3_OK) {
        /* Read mechanism according to selected interface */
        if (dev->intf != BMP3_I2C_INTF) {
            /* If SPI interface is selected, read extra byte */
            dev->dummy_byte = 1;
        } else {
            /* If I2C interface is selected, no need to read
            extra byte */
            dev->dummy_byte = 0;
        }
        /* Read the chip-id of bmp3 sensor */
        rslt = bmp3_get_regs(itf, BMP3_CHIP_ID_ADDR, &chip_id, 1, dev);
        /* Proceed if everything is fine until now */
        if (rslt == BMP3_OK) {
            /* Check for chip id validity */
            if (chip_id == BMP3_CHIP_ID) {
                dev->chip_id = chip_id;
                /* Reset the sensor */
                rslt = bmp3_soft_reset(itf, dev);
                if (rslt == BMP3_OK) {
                    /* Read the calibration data */
                    rslt = get_calib_data(itf, dev);
                } else {
                    BMP388_LOG(ERROR, "******bmp3_init bmp3_soft_reset failed %d\n", rslt);
                }
            } else {
                BMP388_LOG(ERROR, "******bmp3_init get wrong chip ID\n");
                rslt = BMP3_E_DEV_NOT_FOUND;
            }
#if BMP388_DEBUG
            BMP388_LOG(ERROR, "******bmp3_init chip ID  0x%x\n", chip_id);
#endif
        } else {
            BMP388_LOG(ERROR, "******bmp3_init get chip ID failed %d\n", rslt);
        }
    }

    return rslt;
}

/**
* Get chip ID
*
* @param sensor interface
* @param ptr to chip id to be filled up
*/
int
bmp388_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id)
{
    uint8_t reg;
    int rc;
    rc = bmp3_get_regs(itf, BMP3_CHIP_ID_ADDR, &reg, 1, &g_bmp388_dev);
    if (rc) {
        goto err;
    }

    *chip_id = reg;

err:
    return rc;
}

int bmp388_dump(struct sensor_itf *itf)
{
    uint8_t val;
    uint8_t index = 0;
    int rc;

    /* Dump all the register values for debug purposes */
    for (index = 0; index < 0x7F; index++) {
        val = 0;
        rc = bmp3_get_regs(itf, index, &val, 1, &g_bmp388_dev);
        if (rc) {
            BMP388_LOG(ERROR, "read register 0x%02X failed %d\n", index, rc);
            goto err;
        }
        BMP388_LOG(ERROR, "register 0x%02X : 0x%02X\n", index, val);
    }

err:
        return rc;
}

/*!
* @brief This internal API fills the fifo_config_1(fifo_mode,
* fifo_stop_on_full, fifo_time_en, fifo_press_en, fifo_temp_en) settings in the
* reg_data variable so as to burst write in the sensor.
*/
static void fill_fifo_config_1(uint16_t desired_settings, uint8_t *reg_data,
                struct bmp3_fifo_settings *dev_fifo)
{
    if (desired_settings & BMP3_FIFO_MODE_SEL) {
        /* Fill the FIFO mode register data */
        *reg_data = BMP3_SET_BITS_POS_0(*reg_data, BMP3_FIFO_MODE, dev_fifo->mode);
    }
    if (desired_settings & BMP3_FIFO_STOP_ON_FULL_EN_SEL) {
        /* Fill the stop on full data */
        *reg_data = BMP3_SET_BITS(*reg_data, BMP3_FIFO_STOP_ON_FULL, dev_fifo->stop_on_full_en);
    }
    if (desired_settings & BMP3_FIFO_TIME_EN_SEL) {
        /* Fill the time enable data */
        *reg_data = BMP3_SET_BITS(*reg_data, BMP3_FIFO_TIME_EN, dev_fifo->time_en);
    }
    if (desired_settings &
            (BMP3_FIFO_PRESS_EN_SEL | BMP3_FIFO_TEMP_EN_SEL)) {
        /* Fill the FIFO pressure enable */
        if (desired_settings & BMP3_FIFO_PRESS_EN_SEL) {
            if ((dev_fifo->temp_en == 0) && (dev_fifo->press_en == 1)) {
                /* Set the temperature sensor to be enabled */
                dev_fifo->temp_en = 1;
            }
            /* Fill the pressure enable data */
            *reg_data = BMP3_SET_BITS(*reg_data, BMP3_FIFO_PRESS_EN, dev_fifo->press_en);
        }
        /* Temperature should be enabled to get the pressure data */
        *reg_data = BMP3_SET_BITS(*reg_data, BMP3_FIFO_TEMP_EN, dev_fifo->temp_en);
    }

}

/*!
* @brief This internal API fills the fifo_config_2(fifo_subsampling,
* data_select) settings in the reg_data variable so as to burst write
* in the sensor.
*/
static void fill_fifo_config_2(uint16_t desired_settings, uint8_t *reg_data,
                const struct bmp3_fifo_settings *dev_fifo)
{
    if (desired_settings & BMP3_FIFO_DOWN_SAMPLING_SEL) {
        /* To do check Normal mode */
        /* Fill the down-sampling data */
        *reg_data = BMP3_SET_BITS_POS_0(*reg_data, BMP3_FIFO_DOWN_SAMPLING, dev_fifo->down_sampling);
    }
    if (desired_settings & BMP3_FIFO_FILTER_EN_SEL) {
        /* Fill the filter enable data */
        *reg_data = BMP3_SET_BITS(*reg_data, BMP3_FIFO_FILTER_EN, dev_fifo->filter_en);
    }
}

/*!
* @brief This internal API fills the fifo interrupt control(fwtm_en, ffull_en)
* settings in the reg_data variable so as to burst write in the sensor.
*/
static void fill_fifo_int_ctrl(uint16_t desired_settings, uint8_t *reg_data,
                const struct bmp3_fifo_settings *dev_fifo)
{
    if (desired_settings & BMP3_FIFO_FWTM_EN_SEL) {
        /* Fill the FIFO watermark interrupt enable data */
        *reg_data = BMP3_SET_BITS(*reg_data, BMP3_FIFO_FWTM_EN, dev_fifo->fwtm_en);
    }
    if (desired_settings & BMP3_FIFO_FULL_EN_SEL) {
        /* Fill the FIFO full interrupt enable data */
        *reg_data = BMP3_SET_BITS(*reg_data, BMP3_FIFO_FULL_EN, dev_fifo->ffull_en);
    }
}


/*!
* @brief This API sets the fifo_config_1(fifo_mode,
* fifo_stop_on_full, fifo_time_en, fifo_press_en, fifo_temp_en),
* fifo_config_2(fifo_subsampling, data_select) and int_ctrl(fwtm_en, ffull_en)
* settings in the sensor.
*/
int8_t bmp3_set_fifo_settings(struct sensor_itf *itf, uint16_t desired_settings, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t fifo_sett[5];
    uint8_t len = 3;
    uint8_t reg_addr[3] = {BMP3_FIFO_CONFIG_1_ADDR, BMP3_FIFO_CONFIG_1_ADDR+1, BMP3_FIFO_CONFIG_1_ADDR+2};

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    /* Proceed if null check is fine */
    if ((rslt == BMP3_OK) && (dev->fifo != NULL)) {
        rslt = bmp3_get_regs(itf, reg_addr[0], fifo_sett, len, dev);

        if (rslt == BMP3_OK) {
            if (are_settings_changed(FIFO_CONFIG_1, desired_settings)) {
                /* Fill the FIFO config 1 register data */
                fill_fifo_config_1(desired_settings, &fifo_sett[0], &dev->fifo->settings);
            }
            if (are_settings_changed(desired_settings, FIFO_CONFIG_2)) {
                /* Fill the FIFO config 2 register data */
                fill_fifo_config_2(desired_settings, &fifo_sett[1], &dev->fifo->settings);
            }
            if (are_settings_changed(desired_settings, FIFO_INT_CTRL)) {
                /* Fill the FIFO interrupt ctrl register data */
                fill_fifo_int_ctrl(desired_settings, &fifo_sett[2],  &dev->fifo->settings);
            }
            /* Write the FIFO settings in the sensor */
            rslt = bmp3_set_regs(itf, reg_addr, fifo_sett, len, dev);
        }
    } else {
        rslt = BMP3_E_NULL_PTR;
    }

    return rslt;
}

/*!
* @brief This internal API converts the no. of frames required by the user to
* bytes so as to write in the watermark length register.
*/
static int8_t convert_frames_to_bytes(uint16_t *watermark_len, const struct bmp3_dev *dev)
{
    int8_t rslt = BMP3_OK;

    if ((dev->fifo->data.req_frames > 0) && (dev->fifo->data.req_frames <= BMP3_FIFO_MAX_FRAMES)) {
        if (dev->fifo->settings.press_en && dev->fifo->settings.temp_en) {
            /* Multiply with pressure and temperature header len */
            *watermark_len = dev->fifo->data.req_frames * BMP3_P_AND_T_HEADER_DATA_LEN;
        } else if (dev->fifo->settings.temp_en || dev->fifo->settings.press_en) {
            /* Multiply with pressure or temperature header len */
            *watermark_len = dev->fifo->data.req_frames * BMP3_P_OR_T_HEADER_DATA_LEN;
        } else {
            /* No sensor is enabled */
            rslt = BMP3_W_SENSOR_NOT_ENABLED;
        }
    } else {
        /* Required frame count is zero, which is invalid */
        rslt = BMP3_W_INVALID_FIFO_REQ_FRAME_CNT;
    }

    return rslt;
}

/*!
* @brief This API sets the fifo watermark length according to the frames count
* set by the user in the device structure. Refer below for usage.
*/
int8_t bmp3_set_fifo_watermark(struct sensor_itf *itf, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data[2];
    uint8_t reg_addr[2] = {BMP3_FIFO_WM_ADDR, BMP3_FIFO_WM_ADDR+1};
    uint16_t watermark_len;

    rslt = null_ptr_check(dev);

    if ((rslt == BMP3_OK) && (dev->fifo != NULL)) {
        rslt = convert_frames_to_bytes(&watermark_len, dev);
        if (rslt == BMP3_OK) {
            reg_data[0] = BMP3_GET_LSB(watermark_len);
            reg_data[1] = BMP3_GET_MSB(watermark_len) & 0x01;
            rslt = bmp3_set_regs(itf, reg_addr, reg_data, 2, dev);
        }
    }

    return rslt;
}

int8_t bmp388_configure_fifo_with_watermark(struct sensor_itf *itf, struct bmp3_dev *dev, uint8_t en)
{
    int8_t rslt;
    /* FIFO object to be assigned to device structure */
    struct bmp3_fifo fifo;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;

    /* Enable fifo */
    fifo.settings.mode = BMP3_ENABLE;
    /* Enable Pressure sensor for fifo */
    fifo.settings.press_en = BMP3_ENABLE;
    /* Enable temperature sensor for fifo */
    fifo.settings.temp_en = BMP3_ENABLE;
    /* Enable fifo time */
    fifo.settings.time_en = BMP3_ENABLE;
    /* No subsampling for FIFO */
    fifo.settings.down_sampling = BMP3_FIFO_NO_SUBSAMPLING;
    fifo.settings.fwtm_en = en;
    /* Link the fifo object to device structure */
    dev->fifo = &fifo;
    /* Select the settings required for fifo */
    settings_sel = BMP3_FIFO_MODE_SEL | BMP3_FIFO_TIME_EN_SEL | BMP3_FIFO_TEMP_EN_SEL |
        BMP3_FIFO_PRESS_EN_SEL | BMP3_FIFO_DOWN_SAMPLING_SEL | BMP3_FIFO_FWTM_EN_SEL;
    /* Set the selected settings in fifo */
    rslt = bmp3_set_fifo_settings(itf, settings_sel, dev);
    if (rslt != 0) {
    BMP388_LOG(ERROR, "bmp3_set_fifo_settings failed %d\n", rslt);
    goto ERR;
    }
    /* Set the number of frames to be read so as to set the watermark length in the sensor */
    //dev->fifo->data.req_frames = 50;
    dev->fifo->data.req_frames = g_bmp388_dev.fifo_watermark_level;
    rslt = bmp3_set_fifo_watermark(itf, dev);
    if (rslt != 0) {
    BMP388_LOG(ERROR, "bmp3_set_fifo_watermark failed %d\n", rslt);
    goto ERR;
    }
ERR:
    return rslt;

}

int8_t bmp388_configure_fifo_with_fifofull(struct sensor_itf *itf, struct bmp3_dev *dev, uint8_t en)
{
    int8_t rslt;
    /* FIFO object to be assigned to device structure */
    struct bmp3_fifo fifo;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;

    /* Enable fifo */
    fifo.settings.mode = BMP3_ENABLE;
    //fifo.settings.stop_on_full_en = BMP3_ENABLE;
    /* Enable Pressure sensor for fifo */
    fifo.settings.press_en = BMP3_ENABLE;
    /* Enable temperature sensor for fifo */
    fifo.settings.temp_en = BMP3_ENABLE;
    /* Enable fifo time */
    fifo.settings.time_en = BMP3_ENABLE;
    /* No subsampling for FIFO */
    fifo.settings.down_sampling = BMP3_FIFO_NO_SUBSAMPLING;
    fifo.settings.ffull_en = en;
    /* Link the fifo object to device structure */
    dev->fifo = &fifo;
    /* Select the settings required for fifo */
    settings_sel = BMP3_FIFO_MODE_SEL | BMP3_FIFO_TIME_EN_SEL | BMP3_FIFO_TEMP_EN_SEL |
        BMP3_FIFO_PRESS_EN_SEL | BMP3_FIFO_DOWN_SAMPLING_SEL | BMP3_FIFO_FULL_EN_SEL;
    /* Set the selected settings in fifo */
    rslt = bmp3_set_fifo_settings(itf, settings_sel, dev);
    if (rslt != 0) {
    BMP388_LOG(ERROR, "bmp3_set_fifo_settings failed %d\n", rslt);
    goto ERR;
    }
ERR:
    return rslt;

}

int8_t bmp388_enable_fifo(struct sensor_itf *itf, struct bmp3_dev *dev, uint8_t en)
{
    int8_t rslt;
    /* FIFO object to be assigned to device structure */
    struct bmp3_fifo fifo;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;

    /* Enable fifo */
    fifo.settings.mode = en;
    /* Enable Pressure sensor for fifo */
    fifo.settings.press_en = BMP3_ENABLE;
    /* Enable temperature sensor for fifo */
    fifo.settings.temp_en = BMP3_ENABLE;
    /* Enable fifo time */
    fifo.settings.time_en = BMP3_ENABLE;
    /* No subsampling for FIFO */
    fifo.settings.down_sampling = BMP3_FIFO_NO_SUBSAMPLING;
    /* Link the fifo object to device structure */
    dev->fifo = &fifo;
    /* Select the settings required for fifo */
    settings_sel = BMP3_FIFO_MODE_SEL | BMP3_FIFO_TIME_EN_SEL | BMP3_FIFO_TEMP_EN_SEL |
        BMP3_FIFO_PRESS_EN_SEL | BMP3_FIFO_DOWN_SAMPLING_SEL;
    /* Set the selected settings in fifo */
    rslt = bmp3_set_fifo_settings(itf, settings_sel, dev);
    if (rslt != 0) {
    BMP388_LOG(ERROR, "bmp3_set_fifo_settings failed %d\n", rslt);
    goto ERR;
    }
ERR:
    return rslt;

}


/**
* Sets the rate
*
* @param The sensor interface
* @param The rate
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_set_rate(struct sensor_itf *itf, uint8_t rate)
{
    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;
    /* Select the pressure and temperature sensor to be enabled */
    g_bmp388_dev.settings.press_en = BMP3_ENABLE;
    g_bmp388_dev.settings.temp_en = BMP3_ENABLE;
    /* Select the output data rate settings for pressure and temperature */
    //dev->settings.odr_filter.odr = BMP3_ODR_200_HZ;
    g_bmp388_dev.settings.odr_filter.odr = rate;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_ODR_SEL;
    rslt = bmp3_set_sensor_settings(itf, settings_sel, &g_bmp388_dev);
    return rslt;
}

/**
* Gets the rate
*
* @param The sensor ineterface
* @param ptr to the rate
* @return 0 on success, non-zero on failure
*/
int
bmp388_get_rate(struct sensor_itf *itf, uint8_t *rate)
{
    /* todo */
    int rc = 0;


    return rc;
}

/**
* Sets the power mode of the sensor
*
* @param The sensor interface
* @param power mode
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_set_power_mode(struct sensor_itf *itf, uint8_t mode)
{
    /* todo */
    uint8_t rslt = 0;
    /* Select the power mode */
    g_bmp388_dev.settings.op_mode = mode;
    /* Set the power mode in the sensor */
    rslt = bmp3_set_op_mode(itf, &g_bmp388_dev);
    return rslt;
}

/**
* Gets the power mode of the sensor
*
* @param The sensor interface
* @param ptr to power mode setting read from sensor
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_get_power_mode(struct sensor_itf *itf, uint8_t *mode)
{
    /* todo */
    int rc = 0;

    return rc;
}

/**
* Sets the interrupt push-pull/open-drain selection
*
* @param The sensor interface
* @param interrupt setting (0 = push-pull, 1 = open-drain)
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_set_int_pp_od(struct sensor_itf *itf, uint8_t mode)
{

    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;
    /* Select the pressure and temperature sensor to be enabled */
    g_bmp388_dev.settings.press_en = BMP3_ENABLE;
    g_bmp388_dev.settings.temp_en = BMP3_ENABLE;
    /* Select the push-pull or open-drain mode for INT */
    g_bmp388_dev.settings.int_settings.output_mode = mode;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_OUTPUT_MODE_SEL;
    rslt = bmp3_set_sensor_settings(itf, settings_sel, &g_bmp388_dev);
    return rslt;

}

/**
* Gets the interrupt push-pull/open-drain selection
*
* @param The sensor interface
* @param ptr to store setting (0 = push-pull, 1 = open-drain)
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_get_int_pp_od(struct sensor_itf *itf, uint8_t *mode)
{
    /* todo */
    int rc = 0;

    return rc;
}

/**
* Sets whether latched interrupts are enabled
*
* @param The sensor interface
* @param value to set (0 = not latched, 1 = latched)
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_set_latched_int(struct sensor_itf *itf, uint8_t en)
{
    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;
    /* Select the pressure and temperature sensor to be enabled */
    g_bmp388_dev.settings.press_en = BMP3_ENABLE;
    g_bmp388_dev.settings.temp_en = BMP3_ENABLE;
    /* Select latch mode for INT */
    g_bmp388_dev.settings.int_settings.latch = en;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_LATCH_SEL;
    rslt = bmp3_set_sensor_settings(itf, settings_sel, &g_bmp388_dev);
    return rslt;

}

/**
* Gets whether latched interrupts are enabled
*
* @param The sensor interface
* @param ptr to store value (0 = not latched, 1 = latched)
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_get_latched_int(struct sensor_itf *itf, uint8_t *en)
{
    int rc = 0;

    return rc;
}

/**
* Sets whether interrupts are active high or low
*
* @param The sensor interface
* @param value to set (0 = active high, 1 = active low)
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_set_int_active_low(struct sensor_itf *itf, uint8_t low)
{
    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;
    /* Select the pressure and temperature sensor to be enabled */
    g_bmp388_dev.settings.press_en = BMP3_ENABLE;
    g_bmp388_dev.settings.temp_en = BMP3_ENABLE;
    /* Select active level for INT */
    g_bmp388_dev.settings.int_settings.level = low;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_LEVEL_SEL;
    rslt = bmp3_set_sensor_settings(itf, settings_sel, &g_bmp388_dev);
    return rslt;


}

/**
* Gets whether interrupts are active high or low
*
* @param The sensor interface
* @param ptr to store value (0 = active high, 1 = active low)
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_get_int_active_low(struct sensor_itf *itf, uint8_t *low)
{
    int rc = 0;

    return rc;

}

/**
* Sets whether data ready interrupt are enabled
*
* @param The sensor interface
* @param value to set (0 = not latched, 1 = latched)
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_set_drdy_int(struct sensor_itf *itf, uint8_t en)
{
    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;
    /* Select the pressure and temperature sensor to be enabled */
    g_bmp388_dev.settings.press_en = BMP3_ENABLE;
    g_bmp388_dev.settings.temp_en = BMP3_ENABLE;
    /* Select data ready for INT */
    g_bmp388_dev.settings.int_settings.drdy_en = en;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_DRDY_EN_SEL;
    rslt = bmp3_set_sensor_settings(itf, settings_sel, &g_bmp388_dev);
    return rslt;

}


/**
* Set filter config
*
* @param the sensor interface
* @param the filter bandwidth
* @param filter type (1 = high pass, 0 = low pass)
* @return 0 on success, non-zero on failure
*/
int
bmp388_set_filter_cfg(struct sensor_itf *itf, uint8_t press_osr, uint8_t temp_osr)
{
    int8_t rslt = 0;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;
    /* Select the pressure and temperature sensor to be enabled */
    g_bmp388_dev.settings.press_en = BMP3_ENABLE;
    g_bmp388_dev.settings.temp_en = BMP3_ENABLE;
    /* Select the oversampling settings for pressure and temperature */
    g_bmp388_dev.settings.odr_filter.press_os = press_osr;
    g_bmp388_dev.settings.odr_filter.temp_os = temp_osr;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_PRESS_OS_SEL | BMP3_TEMP_OS_SEL;
    /* Write the settings in the sensor */
    rslt = bmp3_set_sensor_settings(itf, settings_sel, &g_bmp388_dev);

    return rslt;

}

/**
* Get filter config
*
* @param the sensor interface
* @param ptr to the filter bandwidth
* @param ptr to filter type (1 = high pass, 0 = low pass)
* @return 0 on success, non-zero on failure
*/
int
bmp388_get_filter_cfg(struct sensor_itf *itf, uint8_t *bw, uint8_t *type)
{
    int rc = 0;

    return rc;
}


/**
* Setup FIFO
*
* @param the sensor interface
* @param FIFO mode to setup
* @param Threshold to set for FIFO
* @return 0 on success, non-zero on failure
*/
int bmp388_set_fifo_cfg(struct sensor_itf *itf, enum bmp388_fifo_mode mode, uint8_t fifo_ths)
{
    int rc = 0;
    g_bmp388_dev.fifo_watermark_level = fifo_ths;
#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
    if (mode == BMP388_FIFO_M_FIFO) {
        rc = bmp388_enable_fifo(itf, &g_bmp388_dev, BMP3_ENABLE);
    } else {
        rc = bmp388_enable_fifo(itf, &g_bmp388_dev, BMP3_DISABLE);
    }
#else
    rc = bmp388_enable_fifo(itf, &g_bmp388_dev, BMP3_DISABLE);

#endif
    return rc;
}

/**
* Clear interrupt 1
*
* @param the sensor interface
*/
int
bmp388_clear_int(struct sensor_itf *itf)
{
    int rslt = 0;
    uint8_t reg_addr;
    uint8_t reg_data;
    reg_addr = BMP3_INT_CTRL_ADDR;
    rslt = bmp3_get_regs(itf, reg_addr, &reg_data, 1, &g_bmp388_dev);
    if (rslt == BMP3_OK) {
        g_bmp388_dev.settings.int_settings.drdy_en = BMP3_DISABLE;
        reg_data = BMP3_SET_BITS(reg_data, BMP3_INT_DRDY_EN, BMP3_DISABLE);
        reg_data = BMP3_SET_BITS(reg_data, BMP3_FIFO_FWTM_EN, BMP3_DISABLE);
        reg_data = BMP3_SET_BITS(reg_data, BMP3_FIFO_FULL_EN, BMP3_DISABLE);
        rslt = bmp3_set_regs(itf, &reg_addr, &reg_data, 1, &g_bmp388_dev);
    }

    return rslt;

}

/**
* Set whether interrupts are enabled
*
* @param the sensor interface
* @param value to set (0 = disabled, 1 = enabled)
* @return 0 on success, non-zero on failure
*/
int bmp388_set_int_enable(struct sensor_itf *itf, uint8_t enabled, uint8_t int_type)
{
    int rc = 0;
    switch (int_type)
    {
    case BMP388_DRDY_INT:
#if BMP388_DEBUG
        BMP388_LOG(ERROR, "*****bmp388_set_int_enable start to set data ready interrupt\n");
#endif
        rc = bmp388_set_drdy_int(itf, enabled);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_set_drdy_int failed %d\n", rc);
            goto ERR;
        }

        rc = bmp388_set_normal_mode(itf, &g_bmp388_dev);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_set_normal_mode failed %d\n", rc);
            goto ERR;
        }
        break;
    case BMP388_FIFO_WTMK_INT:
#if BMP388_DEBUG
        BMP388_LOG(ERROR, "*****bmp388_set_int_enable start to set fifo water mark\n");
#endif
        rc = bmp388_configure_fifo_with_watermark(itf, &g_bmp388_dev, enabled);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_configure_fifo_with_watermark failed %d\n", rc);
            goto ERR;
        }

        rc = bmp388_set_normal_mode(itf, &g_bmp388_dev);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_set_normal_mode failed %d\n", rc);
            goto ERR;
        }
        break;
    case BMP388_FIFO_FULL_INT:
#if BMP388_DEBUG
        BMP388_LOG(ERROR, "*****bmp388_set_int_enable start to set fifo full\n");
#endif
        rc = bmp388_configure_fifo_with_fifofull(itf, &g_bmp388_dev, enabled);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_configure_fifo_with_fifofull failed %d\n", rc);
            goto ERR;
        }
        rc = bmp388_set_normal_mode(itf, &g_bmp388_dev);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_set_normal_mode failed %d\n", rc);
            goto ERR;
        }
        break;
    default:
        rc = SYS_EINVAL;
        BMP388_LOG(ERROR, "******invalid BMP388 interrupt type\n");
        goto ERR;

    }
    return 0;

ERR:
    return rc;
}

/*!
* @brief This API gets the command ready, data ready for pressure and
* temperature, power on reset status from the sensor.
*
* @param[in,out] dev : Structure instance of bmp3_dev
*
* @return Result of API execution status.
* @retval zero -> Success / -ve value -> Error.
*/
static int8_t get_sensor_status(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_addr;
    uint8_t reg_data;

    reg_addr = BMP3_SENS_STATUS_REG_ADDR;
    rslt = bmp3_get_regs(itf, reg_addr, &reg_data, 1, dev);

    if (rslt == BMP3_OK) {
        dev->status.sensor.cmd_rdy = BMP3_GET_BITS(reg_data, BMP3_STATUS_CMD_RDY);
        dev->status.sensor.drdy_press = BMP3_GET_BITS(reg_data, BMP3_STATUS_DRDY_PRESS);
        dev->status.sensor.drdy_temp = BMP3_GET_BITS(reg_data, BMP3_STATUS_DRDY_TEMP);

        reg_addr = BMP3_EVENT_ADDR;
        rslt = bmp3_get_regs(itf, reg_addr, &reg_data, 1, dev);
        dev->status.pwr_on_rst = reg_data & 0x01;
    }

        return rslt;
}

/*!
* @brief This API gets the interrupt (fifo watermark, fifo full, data ready)
* status from the sensor.
*
* @param[in,out] dev : Structure instance of bmp3_dev
*
* @return Result of API execution status.
* @retval zero -> Success / -ve value -> Error.
*/
static int8_t get_int_status(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;

    rslt = bmp3_get_regs(itf, BMP3_INT_STATUS_REG_ADDR, &reg_data, 1, dev);

    if (rslt == BMP3_OK) {
        dev->status.intr.fifo_wm = BMP3_GET_BITS_POS_0(reg_data, BMP3_INT_STATUS_FWTM);
        dev->status.intr.fifo_full = BMP3_GET_BITS(reg_data, BMP3_INT_STATUS_FFULL);
        dev->status.intr.drdy = BMP3_GET_BITS(reg_data, BMP3_INT_STATUS_DRDY);
    }

    return rslt;
}

/*!
* @brief This API gets the fatal, command and configuration error
* from the sensor.
*
* @param[in,out] dev : Structure instance of bmp3_dev
*
* @return Result of API execution status.
* @retval zero -> Success / -ve value -> Error.
*/
static int8_t get_err_status(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;

    rslt = bmp3_get_regs(itf, BMP3_ERR_REG_ADDR, &reg_data, 1, dev);

    if (rslt == BMP3_OK) {
        dev->status.err.cmd = BMP3_GET_BITS_POS_0(reg_data, BMP3_ERR_FATAL);
        dev->status.err.conf = BMP3_GET_BITS(reg_data, BMP3_ERR_CMD);
        dev->status.err.fatal = BMP3_GET_BITS(reg_data, BMP3_ERR_CONF);
    }

    return rslt;
}

/*!
* @brief This API gets the command ready, data ready for pressure and
* temperature and interrupt (fifo watermark, fifo full, data ready) and
* error status from the sensor.
*/
int8_t bmp3_get_status(struct sensor_itf *itf, struct bmp3_dev *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);

    if (rslt == BMP3_OK) {
        rslt = get_sensor_status(itf, dev);
        /* Proceed further if the earlier operation is fine */
        if (rslt == BMP3_OK) {
            rslt = get_int_status(itf, dev);
            /* Proceed further if the earlier operation is fine */
            if (rslt == BMP3_OK) {
                /* Get the error status */
                rslt = get_err_status(itf, dev);
            }
        }
    }

    return rslt;
}

/*!
* @brief This internal API resets the FIFO buffer, start index,
* parsed frame count, configuration change, configuration error and
* frame_not_available variables.
*
* @param[out] fifo : FIFO structure instance where the fifo related variables
* are reset.
*/
static void reset_fifo_index(struct bmp3_fifo *fifo)
{
    /* Loop variable */
    uint16_t i;

    for (i = 0; i < 512; i++) {
        /* Initialize data buffer to zero */
        fifo->data.buffer[i] = 0;
    }
    fifo->data.byte_count = 0;
    fifo->data.start_idx = 0;
    fifo->data.parsed_frames = 0;
    fifo->data.config_change = 0;
    fifo->data.config_err = 0;
    fifo->data.frame_not_available = 0;
}

/*!
* @brief This API gets the fifo length from the sensor.
*/
int8_t bmp3_get_fifo_length(struct sensor_itf *itf, uint16_t *fifo_length, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data[2];

    rslt = null_ptr_check(dev);

    if (rslt == BMP3_OK) {
        rslt = bmp3_get_regs(itf, BMP3_FIFO_LENGTH_ADDR, reg_data, 2, dev);
        /* Proceed if read from register is fine */
        if (rslt == BMP3_OK) {
            /* Update the fifo length */
            *fifo_length = BMP3_CONCAT_BYTES(reg_data[1], reg_data[0]);
        }
    }

    return rslt;
}

/*!
* @brief This API gets the fifo data from the sensor.
*/
int8_t bmp3_get_fifo_data(struct sensor_itf *itf, const struct bmp3_dev *dev)
{
    int8_t rslt;
    uint16_t fifo_len;
    struct bmp3_fifo *fifo = dev->fifo;
#if FIFOPARSE_DEBUG
    uint16_t i;
#endif

    rslt = null_ptr_check(dev);

    if ((rslt == BMP3_OK) && (fifo != NULL)) {
        reset_fifo_index(dev->fifo);
        /* Get the total no of bytes available in FIFO */
        rslt = bmp3_get_fifo_length(itf, &fifo_len, dev);
        BMP388_LOG(ERROR, "*****fifo_len is %d\n", fifo_len);
#if BMP388_DEBUG
        BMP388_LOG(ERROR, "*****fifo_len is %d\n", fifo_len);
#endif
        /* For sensor time frame */
        if (dev->fifo->settings.time_en == TRUE) {
            fifo_len = fifo_len + 4 + 7*3;
#if BMP388_DEBUG
            BMP388_LOG(ERROR, "*****fifo_len added timefifo length is %d\n", fifo_len);
#endif
        }
        /* Update the fifo length in the fifo structure */
        dev->fifo->data.byte_count = fifo_len;
        if (rslt == BMP3_OK) {
            /* Read the fifo data */
            rslt = bmp3_get_regs(itf, BMP3_FIFO_DATA_ADDR, fifo->data.buffer, fifo_len, dev);
        }
#if FIFOPARSE_DEBUG
        if (rslt == 0) {
            for (i = 0; i < fifo_len; i++)
                BMP388_LOG(ERROR, "*****i is %d buffer[i] is %d\n", i, fifo->data.buffer[i]);
        }
#endif
    } else {
        rslt = BMP3_E_NULL_PTR;
    }

    return rslt;
}

/*!
* @brief This internal API parses the FIFO buffer and gets the header info.
*
* @param[out] header : Variable used to store the fifo header data.
* @param[in] fifo_buffer : FIFO buffer from where the header data is retrieved.
* @param[out] byte_index : Byte count of fifo buffer.
*/
static void get_header_info(uint8_t *header, const uint8_t *fifo_buffer, uint16_t *byte_index)
{
    *header = fifo_buffer[*byte_index];
    *byte_index = *byte_index + 1;
}

/*!
* @brief This internal API parses the FIFO data frame from the fifo buffer and
* fills uncompensated temperature and/or pressure data.
*
* @param[in] sensor_comp : Variable used to select either temperature or
* pressure or both while parsing the fifo frames.
* @param[in] fifo_buffer : FIFO buffer where the temperature or pressure or
* both the data to be parsed.
* @param[out] uncomp_data : Uncompensated temperature or pressure or both the
* data after unpacking from fifo buffer.
*/
static void parse_fifo_sensor_data(uint8_t sensor_comp, const uint8_t *fifo_buffer,
                    struct bmp3_uncomp_data *uncomp_data)
{
    /* Temporary variables to store the sensor data */
    uint32_t data_xlsb;
    uint32_t data_lsb;
    uint32_t data_msb;

    /* Store the parsed register values for temperature data */
    data_xlsb = (uint32_t)fifo_buffer[0];
    data_lsb = (uint32_t)fifo_buffer[1] << 8;
    data_msb = (uint32_t)fifo_buffer[2] << 16;

    if (sensor_comp == BMP3_TEMP) {
        /* Update uncompensated temperature */
        uncomp_data->temperature = data_msb | data_lsb | data_xlsb;
    }
    if (sensor_comp == BMP3_PRESS) {
        /* Update uncompensated pressure */
        uncomp_data->pressure = data_msb | data_lsb | data_xlsb;
    }
    if (sensor_comp == (BMP3_TEMP | BMP3_PRESS)) {
        uncomp_data->temperature = data_msb | data_lsb | data_xlsb;
        /* Store the parsed register values for pressure data */
        data_xlsb = (uint32_t)fifo_buffer[3];
        data_lsb = (uint32_t)fifo_buffer[4] << 8;
        data_msb = (uint32_t)fifo_buffer[5] << 16;
        uncomp_data->pressure = data_msb | data_lsb | data_xlsb;
    }
}


/*!
* @brief This internal API unpacks the FIFO data frame from the fifo buffer and
* fills the byte count, uncompensated pressure and/or temperature data.
*
* @param[out] byte_index : Byte count of fifo buffer.
* @param[in] fifo_buffer : FIFO buffer from where the temperature and pressure
* frames are unpacked.
* @param[out] uncomp_data : Uncompensated temperature and pressure data after
* unpacking from fifo buffer.
*/
static void unpack_temp_press_frame(uint16_t *byte_index, const uint8_t *fifo_buffer,
                    struct bmp3_uncomp_data *uncomp_data)
{
    parse_fifo_sensor_data((BMP3_PRESS | BMP3_TEMP), &fifo_buffer[*byte_index], uncomp_data);
    *byte_index = *byte_index + BMP3_P_T_DATA_LEN;
}

/*!
* @brief This internal API unpacks the FIFO data frame from the fifo buffer and
* fills the byte count and uncompensated temperature data.
*
* @param[out] byte_index : Byte count of fifo buffer.
* @param[in] fifo_buffer : FIFO buffer from where the temperature frames
* are unpacked.
* @param[out] uncomp_data : Uncompensated temperature data after unpacking from
* fifo buffer.
*/
static void unpack_temp_frame(uint16_t *byte_index, const uint8_t *fifo_buffer, struct bmp3_uncomp_data *uncomp_data)
{
    parse_fifo_sensor_data(BMP3_TEMP, &fifo_buffer[*byte_index], uncomp_data);
    *byte_index = *byte_index + BMP3_T_DATA_LEN;
}

/*!
* @brief This internal API unpacks the FIFO data frame from the fifo buffer and
* fills the byte count and uncompensated pressure data.
*
* @param[out] byte_index : Byte count of fifo buffer.
* @param[in] fifo_buffer : FIFO buffer from where the pressure frames are
* unpacked.
* @param[out] uncomp_data : Uncompensated pressure data after unpacking from
* fifo buffer.
*/
static void unpack_press_frame(uint16_t *byte_index, const uint8_t *fifo_buffer, struct bmp3_uncomp_data *uncomp_data)
{
    parse_fifo_sensor_data(BMP3_PRESS, &fifo_buffer[*byte_index], uncomp_data);
    *byte_index = *byte_index + BMP3_P_DATA_LEN;
}

/*!
* @brief This internal API unpacks the time frame from the fifo data buffer and
* fills the byte count and update the sensor time variable.
*
* @param[out] byte_index : Byte count of fifo buffer.
* @param[in] fifo_buffer : FIFO buffer from where the sensor time frames
* are unpacked.
* @param[out] sensor_time : Variable used to store the sensor time.
*/
static void unpack_time_frame(uint16_t *byte_index, const uint8_t *fifo_buffer, uint32_t *sensor_time)
{
    uint16_t index = *byte_index;
    uint32_t xlsb = fifo_buffer[index];
    uint32_t lsb = ((uint32_t)fifo_buffer[index + 1]) << 8;
    uint32_t msb = ((uint32_t)fifo_buffer[index + 2]) << 16;

    *sensor_time = msb | lsb | xlsb;
    *byte_index = *byte_index + BMP3_SENSOR_TIME_LEN;
}



/*!
* @brief This internal API parse the FIFO data frame from the fifo buffer and
* fills the byte count, uncompensated pressure and/or temperature data and no
* of parsed frames.
*
* @param[in] header : Pointer variable which stores the fifo settings data
* read from the sensor.
* @param[in,out] fifo : Structure instance of bmp3_fifo which stores the
* read fifo data.
* @param[out] byte_index : Byte count which is incremented according to the
* of data.
* @param[out] uncomp_data : Uncompensated pressure and/or temperature data
* which is stored after parsing fifo buffer data.
* @param[out] parsed_frames : Total number of parsed frames.
*
* @return Result of API execution status.
* @retval zero -> Success / -ve value -> Error
*/
static uint8_t parse_fifo_data_frame(uint8_t header, struct bmp3_fifo *fifo, uint16_t *byte_index,
                    struct bmp3_uncomp_data *uncomp_data, uint8_t *parsed_frames)
{
    uint8_t t_p_frame = 0;
#if FIFOPARSE_DEBUG
    BMP388_LOG(ERROR, "****  header is %d\n",  header);
    BMP388_LOG(ERROR, "****  byte_index is %d\n",  *byte_index);
#endif
    switch (header) {
    case FIFO_TEMP_PRESS_FRAME:
        unpack_temp_press_frame(byte_index, fifo->data.buffer, uncomp_data);
        *parsed_frames = *parsed_frames + 1;
        t_p_frame = BMP3_PRESS | BMP3_TEMP;
#if FIFOPARSE_DEBUG
        BMP388_LOG(ERROR, "**** parsed_frames %d\n", *parsed_frames);
        BMP388_LOG(ERROR, "**** FIFO_TEMP_PRESS_FRAME\n");
#endif
        break;

    case FIFO_TEMP_FRAME:
        unpack_temp_frame(byte_index, fifo->data.buffer, uncomp_data);
        *parsed_frames = *parsed_frames + 1;
        t_p_frame = BMP3_TEMP;
#if FIFOPARSE_DEBUG
        BMP388_LOG(ERROR, "**** parsed_frames %d\n", *parsed_frames);
        BMP388_LOG(ERROR, "**** FIFO_TEMP_FRAME\n");
#endif
        break;

    case FIFO_PRESS_FRAME:
        unpack_press_frame(byte_index, fifo->data.buffer, uncomp_data);
        *parsed_frames = *parsed_frames + 1;
        t_p_frame = BMP3_PRESS;
#if FIFOPARSE_DEBUG
        BMP388_LOG(ERROR, "**** parsed_frames %d\n", *parsed_frames);
        BMP388_LOG(ERROR, "**** FIFO_PRESS_FRAME\n");
#endif
        break;

    case FIFO_TIME_FRAME:
        unpack_time_frame(byte_index, fifo->data.buffer, &fifo->data.sensor_time);
        fifo->no_need_sensortime = true;
        fifo->sensortime_updated = true;
#if FIFOPARSE_DEBUG
        BMP388_LOG(ERROR, "**** FIFO_TIME_FRAME\n");
#endif
        BMP388_LOG(ERROR, "**** FIFO_TIME_FRAME\n");

        break;

    case FIFO_CONFIG_CHANGE:
        fifo->data.config_change = 1;
        *byte_index = *byte_index + 1;
#if FIFOPARSE_DEBUG
        BMP388_LOG(ERROR, "**** FIFO_CONFIG_CHANGE\n");
#endif
        break;

    case FIFO_ERROR_FRAME:
        fifo->data.config_err = 1;
        *byte_index = *byte_index + 1;
#if FIFOPARSE_DEBUG
        BMP388_LOG(ERROR, "**** FIFO_ERROR_FRAME\n");
#endif
        break;

    default:
        fifo->data.config_err = 1;
        *byte_index = *byte_index + 1;
#if FIFOPARSE_DEBUG
        BMP388_LOG(ERROR, "**** unknown FIFO_FRAME\n");
#endif
        break;

    }

    return t_p_frame;
}

/*!
* @brief This API extracts the temperature and/or pressure data from the FIFO
* data which is already read from the fifo.
*/
int8_t bmp3_extract_fifo_data(struct bmp3_data *data, struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t header;
    uint16_t byte_index = dev->fifo->data.start_idx;
    uint8_t parsed_frames = 0;
    uint8_t t_p_frame;
    struct bmp3_uncomp_data uncomp_data;

    rslt = null_ptr_check(dev);

    if ((rslt == BMP3_OK) && (dev->fifo != NULL) && (data != NULL)) {

        while (((!dev->fifo->no_need_sensortime) || (parsed_frames < (dev->fifo->data.req_frames))) && (byte_index < dev->fifo->data.byte_count)) {
            get_header_info(&header, dev->fifo->data.buffer, &byte_index);
            t_p_frame = parse_fifo_data_frame(header, dev->fifo, &byte_index, &uncomp_data, &parsed_frames);
            /* If the frame is pressure and/or temperature data */
            if (t_p_frame != FALSE) {
                /* Compensate temperature and pressure data */
                rslt = compensate_data(t_p_frame, &uncomp_data, &data[parsed_frames-1],
                            &dev->calib_data);
            }
        }
#if BMP388_DEBUG
        BMP388_LOG(ERROR, "******byte_index %d\n", byte_index);
        BMP388_LOG(ERROR, "******parsed_frames %d\n", parsed_frames);
        BMP388_LOG(ERROR, "******dev->fifo->no_need_sensortime %d\n", dev->fifo->no_need_sensortime);
#endif
        /* Check if any frames are parsed in FIFO */
        if (parsed_frames != 0) {
            /* Update the bytes parsed in the device structure */
            dev->fifo->data.start_idx = byte_index;
            dev->fifo->data.parsed_frames += parsed_frames;
        } else {
            /* No frames are there to parse. It is time to
                read the FIFO, if more frames are needed */
            dev->fifo->data.frame_not_available = TRUE;
        }
    } else {
        rslt = BMP3_E_NULL_PTR;
    }

    return rslt;
}


/**
* Run Self test on sensor
*
* @param the sensor interface
* @param pointer to return test result in (0 on pass, non-zero on failure)
*
* @return 0 on sucess, non-zero on failure
*/
int bmp388_run_self_test(struct sensor_itf *itf, int *result)
{
    int rc;
    uint8_t chip_id;
    struct bmp3_data sensor_data;
    float pressure;
    float temperature;
    rc = bmp388_get_chip_id(itf, &chip_id);
    if (rc) {
        *result = -1;
        rc = SYS_EINVAL;
        BMP388_LOG(ERROR, "******read BMP388 chipID failed %d\n", rc);
        return rc;
    }

    if (chip_id != BMP3_CHIP_ID) {
        *result = -1;
        rc = SYS_EINVAL;
        BMP388_LOG(ERROR, "******self_test gets BMP388 chipID failed 0x%x\n", chip_id);
        return rc;
    } else {
        BMP388_LOG(ERROR, "******self_test gets BMP388 chipID 0x%x\n", chip_id);
    }
    rc = bmp388_get_sensor_data(itf, &g_bmp388_dev, &sensor_data);
    if (rc) {
        BMP388_LOG(ERROR, "bmp388_get_sensor_data failed %d\n", rc);
        *result = -1;
        rc = SYS_EINVAL;
        return rc;
    }
    pressure = (float)sensor_data.pressure / 10000;
    temperature = (float)sensor_data.temperature / 100;

    if ((pressure < 300) || (pressure > 1250))
    {
        BMP388_LOG(ERROR, "pressure data abnormal\n");
        *result = -1;
        rc = SYS_EINVAL;
        return rc;
    }
    if ((temperature < -40) || (temperature > 85))
    {
        BMP388_LOG(ERROR, "temperature data abnormal\n");
        *result = -1;
        rc = SYS_EINVAL;
        return rc;
    }

    *result = 0;

    return 0;
}

#if MYNEWT_VAL(BMP388_INT_ENABLE)
static void
init_interrupt(struct bmp388_int *interrupt, struct sensor_int *ints)
{
    os_error_t error;

    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);

    interrupt->active = false;
    interrupt->asleep = false;
    interrupt->ints = ints;
}

static void
undo_interrupt(struct bmp388_int * interrupt)
{
    OS_ENTER_CRITICAL(interrupt->lock);
    interrupt->active = false;
    interrupt->asleep = false;
    OS_EXIT_CRITICAL(interrupt->lock);
}

static int
wait_interrupt(struct bmp388_int *interrupt, uint8_t int_num)
{
    bool wait;
    os_error_t error;

    OS_ENTER_CRITICAL(interrupt->lock);

    /* Check if we did not missed interrupt */
    if (hal_gpio_read(interrupt->ints[int_num].host_pin) ==
                                            interrupt->ints[int_num].active) {
        OS_EXIT_CRITICAL(interrupt->lock);
        return OS_OK;
    }

    if (interrupt->active) {
        interrupt->active = false;
        wait = false;
    } else {
        interrupt->asleep = true;
        wait = true;
    }
    OS_EXIT_CRITICAL(interrupt->lock);

    if (wait) {
        error = os_sem_pend(&interrupt->wait, BMP388_MAX_INT_WAIT);
        if (error == OS_TIMEOUT) {
            return error;
        }
        assert(error == OS_OK);
    }
    return OS_OK;
}

static void
wake_interrupt(struct bmp388_int *interrupt)
{
    bool wake;

    OS_ENTER_CRITICAL(interrupt->lock);
    if (interrupt->asleep) {
        interrupt->asleep = false;
        wake = true;
    } else {
        interrupt->active = true;
        wake = false;
    }
    OS_EXIT_CRITICAL(interrupt->lock);

    if (wake) {
        os_error_t error;

        error = os_sem_release(&interrupt->wait);
        assert(error == OS_OK);
    }
}

static void
bmp388_int_irq_handler(void *arg)
{
    struct sensor *sensor = arg;
    struct bmp388 *bmp388;

    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);

    if(bmp388->pdd.interrupt) {
        wake_interrupt(bmp388->pdd.interrupt);
    }

    sensor_mgr_put_interrupt_evt(sensor);
}

static int
init_intpin(struct bmp388 *bmp388, hal_gpio_irq_handler_t handler,
            void *arg)
{
    hal_gpio_irq_trig_t trig;
    int pin = -1;
    int rc;
    int i;

    for (i = 0; i < MYNEWT_VAL(SENSOR_MAX_INTERRUPTS_PINS); i++){
        pin = bmp388->sensor.s_itf.si_ints[i].host_pin;
        if (pin >= 0) {
            break;
        }
    }

    if (pin < 0) {
        BMP388_LOG(ERROR, "Interrupt pin not configured\n");
        return SYS_EINVAL;
    }

    if (bmp388->sensor.s_itf.si_ints[i].active) {
        trig = HAL_GPIO_TRIG_RISING;
    } else {
        trig = HAL_GPIO_TRIG_FALLING;
    }

    rc = hal_gpio_irq_init(pin,
                        handler,
                        arg,
                        trig,
                        HAL_GPIO_PULL_NONE);
    if (rc != 0) {
        BMP388_LOG(ERROR, "Failed to initialise interrupt pin %d\n", pin);
        return rc;
    }

    return 0;
}

static int
disable_interrupt(struct sensor *sensor, uint8_t int_to_disable, uint8_t int_num)
{
    struct bmp388 *bmp388;
    struct bmp388_pdd *pdd;
    struct sensor_itf *itf;
    int rc = 0;

    if (int_to_disable == 0) {
        return SYS_EINVAL;
    }
    BMP388_LOG(ERROR, "*****disable_interrupt entered \n");

    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &bmp388->pdd;

    pdd->int_enable &= ~(int_to_disable << (int_num * 8));

    /* disable int pin */
    if (!pdd->int_enable) {
        BMP388_LOG(ERROR, "*****disable_interrupt disable int pin \n");
        hal_gpio_irq_disable(itf->si_ints[int_num].host_pin);
        /* disable interrupt in device */
        rc = bmp388_set_int_enable(itf, 0, int_to_disable);
        if (rc) {
            pdd->int_enable |= (int_to_disable << (int_num * 8));
            return rc;
        }
    }

    /* update interrupt setup in device */
    if (int_num == 0) {
    } else {
    }

    return rc;
}

static int
enable_interrupt(struct sensor *sensor, uint8_t int_to_enable, uint8_t int_num)
{
    struct bmp388 *bmp388;
    struct bmp388_pdd *pdd;
    struct sensor_itf *itf;
    int rc;

    if (!int_to_enable) {
        BMP388_LOG(ERROR, "*****enable_interrupt int_to_enable is 0 \n");
        rc = SYS_EINVAL;
        goto err;
    }

    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &bmp388->pdd;

    rc = bmp388_clear_int(itf);
    if (rc) {
        BMP388_LOG(ERROR, "*****enable_interrupt bmp388_clear_int failed%d\n", rc);
        goto err;
    }

    /* if no interrupts are currently in use enable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_enable(itf->si_ints[int_num].host_pin);

        rc = bmp388_set_int_enable(itf, 1, int_to_enable);
        if (rc) {
            BMP388_LOG(ERROR, "*****enable_interrupt bmp388_set_int_enable failed%d\n", rc);
            goto err;
        }
    }

    pdd->int_enable |= (int_to_enable << (int_num * 8));

    /* enable interrupt in device */
    /* enable drdy or fifowartermark or fifofull*/
    if (int_num == 0) {
    } else {
    }

    if (rc) {
        BMP388_LOG(ERROR, "*****enable_interrupt bmp388_set_int1/int2_pin_cfg failed%d\n", rc);
        disable_interrupt(sensor, int_to_enable, int_num);
        goto err;
    }

    return 0;
err:
    return rc;
}
#endif
static int bmp388_do_report(struct sensor *sensor, sensor_type_t sensor_type, sensor_data_func_t data_func,
                            void * data_arg, struct bmp3_data *data)
{
    int rc;
    float pressure;
    float temperature;
    union {
        struct sensor_temp_data std;
        struct sensor_press_data spd;
    } databuf;

    pressure = (float)data->pressure / 100;
    temperature = (float)data->temperature / 100;

    if (sensor_type & SENSOR_TYPE_PRESSURE)
    {
        databuf.spd.spd_press = pressure;
        databuf.spd.spd_press_is_valid = 1;
        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.spd, SENSOR_TYPE_PRESSURE);
        if (rc) {
            goto err;
        }
    }

    if (sensor_type & SENSOR_TYPE_TEMPERATURE)
    {
        databuf.std.std_temp = temperature;
        databuf.std.std_temp_is_valid = 1;
        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.std, SENSOR_TYPE_TEMPERATURE);
        if (rc) {
            goto err;
        }

    }

    return 0;
err:
    return rc;

}

/**
* Do pressure polling reads
*
* @param The sensor ptr
* @param The sensor type
* @param The function pointer to invoke for each accelerometer reading.
* @param The opaque pointer that will be passed in to the function.
* @param If non-zero, how long the stream should run in milliseconds.
*
* @return 0 on success, non-zero on failure.
*/
int
bmp388_poll_read(struct sensor *sensor, sensor_type_t sensor_type,
                sensor_data_func_t data_func, void *data_arg,
                uint32_t timeout)
{
    struct bmp388 *bmp388;
    struct bmp388_cfg *cfg;
    struct sensor_itf *itf;
    int rc;
    struct bmp3_data sensor_data;

    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    cfg = &bmp388->cfg;

    /* If the read isn't looking for pressure data, don't do anything. */
    if ((!(sensor_type & SENSOR_TYPE_PRESSURE)) && (!(sensor_type & SENSOR_TYPE_TEMPERATURE))) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (cfg->read_mode.mode != BMP388_READ_M_POLL) {
        rc = SYS_EINVAL;
        goto err;
    }

    g_bmp388_dev.settings.op_mode = BMP3_FORCED_MODE;
    rc = bmp388_set_forced_mode_with_osr(itf, &g_bmp388_dev);
    if (rc) {
        BMP388_LOG(ERROR, "bmp388_set_forced_mode_with_osr failed %d\n", rc);
        goto err;
    }

    rc = bmp388_get_sensor_data(itf, &g_bmp388_dev, &sensor_data);
    if (rc) {
        BMP388_LOG(ERROR, "bmp388_get_sensor_data failed %d\n", rc);
        goto err;
    }

    rc = bmp388_do_report(sensor, sensor_type, data_func, data_arg, &sensor_data);
    if (rc) {
        BMP388_LOG(ERROR, "bmp388_do_report failed %d\n", rc);
        goto err;
    }

err:
    /* set powermode to original */
    rc = bmp388_set_power_mode(itf, cfg->power_mode);
    return rc;
}

int
bmp388_stream_read(struct sensor *sensor,
                    sensor_type_t sensor_type,
                    sensor_data_func_t read_func,
                    void *read_arg,
                    uint32_t time_ms)
{
#if MYNEWT_VAL(BMP388_INT_ENABLE)
    struct bmp388_pdd *pdd;
#endif
    struct bmp388 *bmp388;
    struct sensor_itf *itf;
    struct bmp388_cfg *cfg;
    os_time_t time_ticks;
    os_time_t stop_ticks = 0;
    int rc;


#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
    /* FIFO object to be assigned to device structure */
    struct bmp3_fifo fifo;
    /* Pressure and temperature array of structures with maximum frame size */
    struct bmp3_data sensor_data[74];
    /* Loop Variable */
    uint8_t i;
    uint16_t frame_length;
    /* try count for polling the watermark interrupt status */
    uint16_t try_count;
    /* Enable fifo */
    fifo.settings.mode = BMP3_ENABLE;
    /* Enable Pressure sensor for fifo */
    fifo.settings.press_en = BMP3_ENABLE;
    /* Enable temperature sensor for fifo */
    fifo.settings.temp_en = BMP3_ENABLE;
    /* Enable fifo time */
    fifo.settings.time_en = BMP3_ENABLE;
    /* No subsampling for FIFO */
    fifo.settings.down_sampling = BMP3_FIFO_NO_SUBSAMPLING;
    fifo.sensortime_updated = false;
#else
    struct bmp3_data sensor_data;

#endif

    /* If the read isn't looking for pressure or temperature data, don't do anything. */
    if ((!(sensor_type & SENSOR_TYPE_PRESSURE)) && (!(sensor_type & SENSOR_TYPE_TEMPERATURE))) {
        BMP388_LOG(ERROR, "unsupported sensor type for bmp388\n");
        return SYS_EINVAL;
    }

    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
#if MYNEWT_VAL(BMP388_INT_ENABLE)
    pdd = &bmp388->pdd;
#endif
    cfg = &bmp388->cfg;


    if (cfg->read_mode.mode != BMP388_READ_M_STREAM) {
        BMP388_LOG(ERROR, "*****bmp388_stream_read mode is not stream\n");
        return SYS_EINVAL;
    }

#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
#if MYNEWT_VAL(BMP388_INT_ENABLE)
    if (cfg->int_enable_type == BMP388_FIFO_WTMK_INT) {
    /* FIFO watemrmark interrupt enable */
    fifo.settings.fwtm_en = BMP3_ENABLE;
    fifo.data.req_frames = g_bmp388_dev.fifo_watermark_level;
    } else if (cfg->int_enable_type == BMP388_FIFO_FULL_INT) {
    /* FIFO watemrmark interrupt enable */
    fifo.settings.ffull_en= BMP3_ENABLE;
    }
#endif
#endif

#if MYNEWT_VAL(BMP388_INT_ENABLE)
    undo_interrupt(&bmp388->intr);

    if (pdd->interrupt) {
        BMP388_LOG(ERROR, "*****bmp388_stream_read interrupt is not null\n");
        return SYS_EBUSY;
    }

    /* enable interrupt */
    pdd->interrupt = &bmp388->intr;
    /* enable and register interrupt according to interrupt type */
    rc = enable_interrupt(sensor, cfg->read_mode.int_type,
                        cfg->read_mode.int_num);
    if (rc) {
        BMP388_LOG(ERROR, "*****bmp388_stream_read enable_interrupt failed%d\n", rc);
        return rc;
    }
#else
#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
    /* no interrupt feature */
    /* enable normal mode for fifo feature */
    rc = bmp388_set_normal_mode(itf, &g_bmp388_dev);
    if (rc) {
        BMP388_LOG(ERROR, "******bmp388_set_normal_mode failed %d\n", rc);
        goto err;
    }
#endif
#endif

#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
    g_bmp388_dev.fifo = &fifo;

    g_bmp388_dev.fifo->data.req_frames = g_bmp388_dev.fifo_watermark_level;

#endif

    if (time_ms != 0) {
        if (time_ms > BMP388_MAX_STREAM_MS)
            time_ms = BMP388_MAX_STREAM_MS;
        rc = os_time_ms_to_ticks(time_ms, &time_ticks);
        if (rc) {
            goto err;
        }
        stop_ticks = os_time_get() + time_ticks;
    }


    for (;;) {

#if MYNEWT_VAL(BMP388_INT_ENABLE)
        rc = wait_interrupt(&bmp388->intr, cfg->read_mode.int_num);
        if (rc) {
            BMP388_LOG(ERROR, "*****bmp388_stream_read wait_interrupt failed%d\n", rc);
            goto err;
        } else {
            BMP388_LOG(ERROR, "*****wait_interrupt got the interrupt\n");
        }
#endif

#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
try_count = 0xFFFF;
#endif

#if MYNEWT_VAL(BMP388_INT_ENABLE)
#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
        /* till watermark level is reached in fifo and fifo interrupt happened */
        do {
            rc = bmp3_get_status(itf, &g_bmp388_dev);
            try_count--;
        } while (((g_bmp388_dev.status.intr.fifo_wm == 0) &&
        (g_bmp388_dev.status.intr.fifo_full== 0)) && (try_count > 0));
#else
        rc = bmp3_get_status(itf, &g_bmp388_dev);
#endif
#else
        /* no interrupt feature */
        /* wait 2ms */
        delay_msec(2);
#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
        try_count--;
#endif
#endif


#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
        if (try_count > 0) {
#if FIFOPARSE_DEBUG
            BMP388_LOG(ERROR, "*****try_count is %d\n", try_count);
#endif
            rc = bmp3_get_fifo_data(itf, &g_bmp388_dev);
            if (fifo.settings.time_en)
                g_bmp388_dev.fifo->no_need_sensortime = false;
            else
                g_bmp388_dev.fifo->no_need_sensortime = true;
            rc = bmp3_extract_fifo_data(sensor_data, &g_bmp388_dev);

            if (g_bmp388_dev.fifo->data.frame_not_available)
            {
                /* no valid fifo frame in sensor */
                BMP388_LOG(ERROR, "**** fifo frames not valid %d\n", rc);
            } else {
#if BMP388_DEBUG
                BMP388_LOG(ERROR, "*****parsed_frames is %d\n", g_bmp388_dev.fifo->data.parsed_frames);
#endif
                frame_length = g_bmp388_dev.fifo->data.req_frames;
                if (frame_length > g_bmp388_dev.fifo->data.parsed_frames) {
                    frame_length = g_bmp388_dev.fifo->data.parsed_frames;
                }

                for (i = 0; i < frame_length; i++)
                {
                    rc = bmp388_do_report(sensor, sensor_type, read_func, read_arg, &sensor_data[i]);
                    if (rc) {
                        BMP388_LOG(ERROR, "bmp388_do_report failed %d\n", rc);
                        goto err;
                    }
                }

                if (g_bmp388_dev.fifo->sensortime_updated) {
                    BMP388_LOG(ERROR, "*****bmp388 sensor time %d\n", g_bmp388_dev.fifo->data.sensor_time);
                    g_bmp388_dev.fifo->sensortime_updated = false;
                }
            }
        }else {

            BMP388_LOG(ERROR, "FIFO water mark unreached\n");
            rc = SYS_EINVAL;
            goto err;
        }

#else
        if (bmp388->cfg.fifo_mode == BMP388_FIFO_M_BYPASS) {
            g_bmp388_dev.settings.op_mode = BMP3_FORCED_MODE;
            rc = bmp388_set_forced_mode_with_osr(itf, &g_bmp388_dev);
            if (rc) {
                BMP388_LOG(ERROR, "bmp388_set_forced_mode_with_osr failed %d\n", rc);
                goto err;
            }
            rc = bmp388_get_sensor_data(itf, &g_bmp388_dev, &sensor_data);
            if (rc) {
                BMP388_LOG(ERROR, "bmp388_get_sensor_data failed %d\n", rc);
                goto err;
            }

            rc = bmp388_do_report(sensor, sensor_type, read_func, read_arg, &sensor_data);
            if (rc) {
                BMP388_LOG(ERROR, "bmp388_do_report failed %d\n", rc);
                goto err;
            }

        }
#endif

        if (time_ms != 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
            BMP388_LOG(INFO, "stream time expired\n");
            BMP388_LOG(INFO, "you can make BMP388_MAX_STREAM_MS bigger to extend stream time duration\n");
            break;
        }

    }

err:

#if MYNEWT_VAL(BMP388_INT_ENABLE)
    /* disable interrupt */
    pdd->interrupt = NULL;
    rc = disable_interrupt(sensor, cfg->read_mode.int_type,
                        cfg->read_mode.int_num);
#endif

    /* set powermode to original */
    rc = bmp388_set_power_mode(itf, cfg->power_mode);

        return rc;
}

static int
bmp388_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int rc;
    const struct bmp388_cfg *cfg;
    struct bmp388 *bmp388;
    struct sensor_itf *itf;
#if BMP388_DEBUG
    BMP388_LOG(ERROR, "bmp388_sensor_read entered\n");
#endif
    /* If the read isn't looking for pressure data, don't do anything. */
    if ((!(type & SENSOR_TYPE_PRESSURE)) && (!(type & SENSOR_TYPE_TEMPERATURE))) {
        rc = SYS_EINVAL;
        BMP388_LOG(ERROR, "bmp388_sensor_read unsupported sensor type\n");
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    if (itf->si_type == SENSOR_ITF_SPI) {

        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_bmp388_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
            * fail if the spi is already enabled
            */
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }
    }
#else
    ((void)itf);
#endif

    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);
    cfg = &bmp388->cfg;

    if (cfg->read_mode.mode == BMP388_READ_M_POLL) {
        rc = bmp388_poll_read(sensor, type, data_func, data_arg, timeout);
    } else {
        rc = bmp388_stream_read(sensor, type, data_func, data_arg, timeout);
    }
err:
    if (rc) {
        BMP388_LOG(ERROR, "bmp388_sensor_read read failed\n");
        return SYS_EINVAL;
    } else {
        return SYS_EOK;
    }
}

static int
bmp388_sensor_set_notification(struct sensor *sensor, sensor_event_type_t event)
{
#if MYNEWT_VAL(BMP388_INT_ENABLE)

    struct bmp388 *bmp388;
    struct bmp388_pdd *pdd;
    int rc;

    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);
    pdd = &bmp388->pdd;

    rc = enable_interrupt(sensor, bmp388->cfg.int_enable_type, MYNEWT_VAL(BMP388_INT_NUM));
    if (rc) {
        goto err;
    }

    pdd->notify_ctx.snec_evtype |= event;

    return 0;
err:
    return rc;
#else

    return 0;

#endif
}

static int
bmp388_sensor_unset_notification(struct sensor *sensor, sensor_event_type_t event)
{

#if MYNEWT_VAL(BMP388_INT_ENABLE)

    struct bmp388 *bmp388;
    int rc;

    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);

    bmp388->pdd.notify_ctx.snec_evtype &= ~event;

    rc = disable_interrupt(sensor, bmp388->cfg.int_enable_type, MYNEWT_VAL(BMP388_INT_NUM));

    return rc;
#else

    return 0;

#endif
}

static int
bmp388_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct bmp388 *bmp388;

    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);

    return bmp388_config(bmp388, (struct bmp388_cfg*)cfg);
}

static int
bmp388_sensor_handle_interrupt(struct sensor *sensor)
{
#if MYNEWT_VAL(BMP388_INT_ENABLE)
#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
    struct bmp388 *bmp388;
#endif
    struct sensor_itf *itf;
    uint8_t int_status_all;
    int rc;
#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
    /* FIFO object to be assigned to device structure */
    struct bmp3_fifo fifo;
    g_bmp388_dev.fifo = &fifo;
#endif

#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
    bmp388 = (struct bmp388 *)SENSOR_GET_DEVICE(sensor);
#endif
    itf = SENSOR_GET_ITF(sensor);

    BMP388_LOG(ERROR, "******bmp388_sensor_handle_interrupt entered\n");

    rc = bmp3_get_status(itf, &g_bmp388_dev);
    if (rc) {
        BMP388_LOG(ERROR, "Could not get status err=0x%02x\n", rc);
        goto err;
    }

#if MYNEWT_VAL(BMP388_FIFO_ENABLE)
    if ((bmp388->cfg.int_enable_type == BMP388_FIFO_WTMK_INT) ||
        (bmp388->cfg.int_enable_type == BMP388_FIFO_FULL_INT)) {
        rc = bmp3_fifo_flush(itf, &g_bmp388_dev);
        if (rc) {
            BMP388_LOG(ERROR, "fifo flush failed, err=0x%02x\n", rc);
            goto err;
        }
    }
#endif

    int_status_all = g_bmp388_dev.status.intr.fifo_wm |
                     g_bmp388_dev.status.intr.fifo_full |
                     g_bmp388_dev.status.intr.drdy;

    if (int_status_all == 0) {
        BMP388_LOG(ERROR, "Could not get any INT happened status \n");
        rc = SYS_EINVAL;
        goto err;
    }
#if CLEAR_INT_AFTER_ISR
    rc = bmp388_clear_int(itf);
    if (rc) {
        BMP388_LOG(ERROR, "Could not clear int src err=0x%02x\n", rc);
        goto err;
    }
#endif

    return 0;
err:
    return rc;
#else
    return SYS_ENODEV;
#endif
}

static int
bmp388_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return 0;
err:
    return rc;
}

/**
* Expects to be called back through os_dev_create().
*
* @param The device object associated with this accelerometer
* @param Argument passed to OS device init, unused
*
* @return 0 on success, non-zero error on failure.
*/
int
bmp388_init(struct os_dev *dev, void *arg)
{
    struct bmp388 *bmp388;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

#if BMP388_DEBUG
    BMP388_LOG(ERROR, "******bmp388_init entered\n");
#endif
    bmp388 = (struct bmp388 *) dev;

    bmp388->cfg.mask = SENSOR_TYPE_ALL;

    sensor = &bmp388->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(bmp388->stats),
        STATS_SIZE_INIT_PARMS(bmp388->stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(bmp388_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(bmp388->stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc) {
        goto err;
    }

    /* Add the light driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_TEMPERATURE |
            SENSOR_TYPE_PRESSURE,
            (struct sensor_driver *) &g_bmp388_sensor_driver);

    if (rc) {
        goto err;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc) {
        goto err;
    }

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    if (sensor->s_itf.si_type == SENSOR_ITF_SPI) {

        g_bmp388_dev.intf = BMP3_SPI_INTF;
        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_init hal_spi_disable failed, rc = %d\n", rc);
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_bmp388_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
            * fail if the spi is already enabled
            */
            BMP388_LOG(ERROR, "******bmp388_init hal_spi_config failed, rc = %d\n", rc);
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_init hal_spi_enable failed, rc = %d\n", rc);
            goto err;
        }

        rc = hal_gpio_init_out(sensor->s_itf.si_cs_pin, 1);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_init hal_gpio_init_out failed, rc = %d\n", rc);
            goto err;
        }

    }else {
        g_bmp388_dev.intf = BMP3_I2C_INTF;

#if BMP388_DEBUG
        BMP388_LOG(ERROR, "******bmp388_init entered\n");
#endif

    }
#endif

#if MYNEWT_VAL(BMP388_INT_ENABLE)

    init_interrupt(&bmp388->intr, bmp388->sensor.s_itf.si_ints);

    bmp388->pdd.notify_ctx.snec_sensor = sensor;
    bmp388->pdd.interrupt = NULL;

    rc = init_intpin(bmp388, bmp388_int_irq_handler, sensor);
    if (rc) {
        BMP388_LOG(ERROR, "******init_intpin failed \n");
        return rc;
    }

#endif

#if BMP388_DEBUG
    BMP388_LOG(ERROR, "******bmp388_init exited \n");
#endif
    return 0;
err:
    return rc;

}

/**
* Configure the sensor
*
* @param ptr to sensor driver
* @param ptr to sensor driver config
*/
int
bmp388_config(struct bmp388 *bmp388, struct bmp388_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;
    uint8_t chip_id;
    struct sensor *sensor;
    itf = SENSOR_GET_ITF(&(bmp388->sensor));
    sensor = &(bmp388->sensor);

#if BMP388_DEBUG
    BMP388_LOG(ERROR, "******bmp388_config entered\n");
#endif

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    if (itf->si_type == SENSOR_ITF_SPI) {

        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_bmp388_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
            * fail if the spi is already enabled
            */
            BMP388_LOG(ERROR, "******bmp388_config hal_spi_config failed, rc = %d\n", rc);
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            BMP388_LOG(ERROR, "******bmp388_config hal_spi_enable failed, rc = %d\n", rc);
            goto err;
        }
    }
#else
    ((void)(sensor));
#endif

    rc = bmp3_init(itf, &g_bmp388_dev);
    if (rc) {
        BMP388_LOG(ERROR, "******config bmp3_init failed %d\n", rc);
        goto err;
    }

    rc = bmp388_get_chip_id(itf, &chip_id);
    if (rc) {
        goto err;
    }

    if (chip_id != BMP3_CHIP_ID) {
        rc = SYS_EINVAL;
        BMP388_LOG(ERROR, "******config gets BMP388 chipID failed 0x%x\n", chip_id);
        goto err;
    } else {
        BMP388_LOG(ERROR, "******config gets BMP388 chipID 0x%x\n", chip_id);
    }

    rc = bmp388_set_int_pp_od(itf, cfg->int_pp_od);
    if (rc) {
        goto err;
    }
    bmp388->cfg.int_pp_od = cfg->int_pp_od;

    rc = bmp388_set_latched_int(itf, cfg->int_latched);
    if (rc) {
        goto err;
    }
    bmp388->cfg.int_latched = cfg->int_latched;

    rc = bmp388_set_int_active_low(itf, cfg->int_active_low);
    if (rc) {
        goto err;
    }
    bmp388->cfg.int_active_low = cfg->int_active_low;

    rc = bmp388_set_filter_cfg(itf, cfg->filter_press_osr, cfg->filter_temp_osr);
    if (rc) {
        goto err;
    }

    /* filter.press_os filter.temp_os */
    bmp388->cfg.filter_press_osr = cfg->filter_press_osr;
    bmp388->cfg.filter_temp_osr =  cfg->filter_temp_osr;

    rc = bmp388_set_rate(itf, cfg->rate);
    if (rc) {
        goto err;
    }

    bmp388->cfg.rate = cfg->rate;

    rc = bmp388_set_power_mode(itf, cfg->power_mode);
    if (rc) {
        goto err;
    }

    bmp388->cfg.power_mode = cfg->power_mode;

    rc = bmp388_set_fifo_cfg(itf, cfg->fifo_mode, cfg->fifo_threshold);
    if (rc) {
        goto err;
    }

    bmp388->cfg.fifo_mode = cfg->fifo_mode;
    bmp388->cfg.fifo_threshold = cfg->fifo_threshold;

    bmp388->cfg.int_enable_type = cfg->int_enable_type;

    rc = sensor_set_type_mask(&(bmp388->sensor), cfg->mask);
    if (rc) {
        goto err;
    }

    bmp388->cfg.read_mode.int_type = cfg->read_mode.int_type;
    bmp388->cfg.read_mode.int_num = cfg->read_mode.int_num;
    bmp388->cfg.read_mode.mode = cfg->read_mode.mode;

    bmp388->cfg.mask = cfg->mask;

#if BMP388_DEBUG
    BMP388_LOG(ERROR, "******bmp388_config exited\n");
#endif

    return 0;
err:
    return rc;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
init_node_cb(struct bus_node *bnode, void *arg)
{
    struct sensor_itf *itf = arg;

    bmp388_init((struct os_dev *)bnode, itf);
}

int
bmp388_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                            const struct bus_i2c_node_cfg *i2c_cfg,
                            struct sensor_itf *sensor_itf)
{
    struct bmp388 *dev = (struct bmp388 *)node;
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    dev->node_is_spi = false;

    sensor_itf->si_dev = &node->bnode.odev;
    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_i2c_node_create(name, node, i2c_cfg, sensor_itf);

    return rc;
}

int
bmp388_create_spi_sensor_dev(struct bus_spi_node *node, const char *name,
                            const struct bus_spi_node_cfg *spi_cfg,
                            struct sensor_itf *sensor_itf)
{
    struct bmp388 *dev = (struct bmp388 *)node;
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    dev->node_is_spi = true;

    sensor_itf->si_dev = &node->bnode.odev;
    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_spi_node_create(name, node, spi_cfg, sensor_itf);

    return rc;
}
#endif
