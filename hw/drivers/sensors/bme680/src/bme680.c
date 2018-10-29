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

#include "os/mynewt.h"
#include "sensor/sensor.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"
#include "sensor/humidity.h"
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "hal/hal_gpio.h"

#include "bme680/bme680.h"

#define BME680_SENSOR_TYPE_GAS_RESISTANCE   \
            MYNEWT_VAL(BME680_GAS_RESISTANCE_SENSOR_TYPE)

/* Uncomment to compile in unused functions */
/*#define ENABLE_UNUSED_FUNCTIONS*/

/*!
 *  @brief This API is the entry point.
 *  It reads the chip-id and calibration data from the sensor.
 *
 *  @param[in,out] dev : Structure instance of bme680_cfg
 *
 *  @return Result of API execution status
 *  @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t bme680_internal_init(struct bme680_cfg *dev);

/*!
 * @brief This API writes the given data to the register address
 * of the sensor.
 *
 * @param[in] reg_addr : Register address from where the data to be written.
 * @param[in] reg_data : Pointer to data buffer which is to be written
 * in the sensor.
 * @param[in] len : No of bytes of data to write..
 * @param[in] dev : Structure instance of bme680_cfg.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t bme680_set_regs(const uint8_t *reg_addr, const uint8_t *reg_data,
                                uint8_t len, struct bme680_cfg *dev);

/*!
 * @brief This API reads the data from the given register address of the sensor.
 *
 * @param[in] reg_addr : Register address from where the data to be read
 * @param[out] reg_data : Pointer to data buffer to store the read data.
 * @param[in] len : No of bytes of data to be read.
 * @param[in] dev : Structure instance of bme680_cfg.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t bme680_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint16_t len,
                                struct bme680_cfg *dev);

/*!
 * @brief This API performs the soft reset of the sensor.
 *
 * @param[in] dev : Structure instance of bme680_cfg.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error.
 */
static int8_t bme680_soft_reset(struct bme680_cfg *dev);

/*!
 * @brief This API is used to set the power mode of the sensor.
 *
 * @param[in] dev : Structure instance of bme680_cfg
 * @note : Pass the value to bme680_cfg.power_mode structure variable.
 *
 *  value	|	mode
 * -------------|------------------
 *	0x00	|	BME680_SLEEP_MODE
 *	0x01	|	BME680_FORCED_MODE
 *
 * * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t bme680_set_sensor_mode(struct bme680_cfg *dev);

/*!
 * @brief This API is used to get the power mode of the sensor.
 *
 * @param[in] dev : Structure instance of bme680_cfg
 * @note : bme680_cfg.power_mode structure variable hold the power mode.
 *
 *  value	|	mode
 * ---------|------------------
 *	0x00	|	BME680_SLEEP_MODE
 *	0x01	|	BME680_FORCED_MODE
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t bme680_get_sensor_mode(struct bme680_cfg *dev);

/*!
 * @brief This API is used to set the profile duration of the sensor.
 *
 * @param[in] dev	   : Structure instance of bme680_cfg.
 * @param[in] duration : Duration of the measurement in ms.
 *
 * @return Nothing
 */
#ifdef ENABLE_UNUSED_FUNCTIONS
static void bme680_set_profile_dur(uint16_t duration,
                                    struct bme680_cfg *dev);
#endif

/*!
 * @brief This API is used to get the profile duration of the sensor.
 *
 * @param[in] dev	   : Structure instance of bme680_cfg.
 * @param[in] duration : Duration of the measurement in ms.
 *
 * @return Nothing
 */
static void bme680_get_profile_dur(uint16_t *duration,
                                    const struct bme680_cfg *dev);

/*!
 * @brief This API reads the pressure, temperature and humidity and gas data
 * from the sensor, compensates the data and store it in the bme680_data
 * structure instance passed by the user.
 *
 * @param[out] data: Structure instance to hold the data.
 * @param[in] dev : Structure instance of bme680_cfg.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t bme680_get_sensor_data(struct bme680_field_data *data,
                                        struct bme680_cfg *dev);

/*!
 * @brief This API is used to set the oversampling, filter and T,P,H, gas selection
 * settings in the sensor.
 *
 * @param[in] dev : Structure instance of bme680_cfg.
 * @param[in] desired_settings : Variable used to select the settings which
 * are to be set in the sensor.
 *
 *	 Macros	                   |  Functionality
 *---------------------------------|----------------------------------------------
 *	BME680_OST_SEL             |    To set temperature oversampling.
 *	BME680_OSP_SEL             |    To set pressure oversampling.
 *	BME680_OSH_SEL             |    To set humidity oversampling.
 *	BME680_GAS_MEAS_SEL        |    To set gas measurement setting.
 *	BME680_FILTER_SEL          |    To set filter setting.
 *	BME680_HCNTRL_SEL          |    To set humidity control setting.
 *	BME680_RUN_GAS_SEL         |    To set run gas setting.
 *	BME680_NBCONV_SEL          |    To set NB conversion setting.
 *	BME680_GAS_SENSOR_SEL      |    To set all gas sensor related settings
 *
 * @note : Below are the macros to be used by the user for selecting the
 * desired settings. User can do OR operation of these macros for configuring
 * multiple settings.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error.
 */
static int8_t bme680_set_sensor_settings(uint16_t desired_settings,
                                            struct bme680_cfg *dev);

/*!
 * @brief This API is used to get the oversampling, filter and T,P,H, gas selection
 * settings in the sensor.
 *
 * @param[in] dev : Structure instance of bme680_cfg.
 * @param[in] desired_settings : Variable used to select the settings which
 * are to be get from the sensor.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error.
 */
#ifdef ENABLE_UNUSED_FUNCTIONS
static int8_t bme680_get_sensor_settings(uint16_t desired_settings,
                                            struct bme680_cfg *dev);
#endif

/*!
 * @brief This internal API is used to read the calibrated data from the sensor.
 *
 * This function is used to retrieve the calibration
 * data from the image registers of the sensor.
 *
 * @note Registers 89h  to A1h for calibration data 1 to 24
 *        from bit 0 to 7
 * @note Registers E1h to F0h for calibration data 25 to 40
 *        from bit 0 to 7
 * @param[in] dev   :Structure instance of bme680_cfg.
 *
 * @return Result of API execution status.
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t get_calib_data(struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to set the gas configuration of the sensor.
 *
 * @param[in] dev   :Structure instance of bme680_cfg.
 *
 * @return Result of API execution status.
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t set_gas_config(struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to get the gas configuration of the sensor.
 * @note heatr_temp and heatr_dur values are currently register data
 * and not the actual values set
 *
 * @param[in] dev   :Structure instance of bme680_cfg.
 *
 * @return Result of API execution status.
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
#ifdef ENABLE_UNUSED_FUNCTIONS
static int8_t get_gas_config(struct bme680_cfg *dev);
#endif

/*!
 * @brief This internal API is used to calculate the Heat duration value.
 *
 * @param[in] dur   :Value of the duration to be shared.
 *
 * @return uint8_t threshold duration after calculation.
 */
static uint8_t calc_heater_dur(uint16_t dur);

#ifndef BME680_FLOAT_POINT_COMPENSATION

/*!
 * @brief This internal API is used to calculate the temperature value.
 *
 * @param[in] dev       :Structure instance of bme680_cfg.
 * @param[in] temp_adc  :Contains the temperature ADC value .
 *
 * @return uint32_t calculated temperature.
 */
static int16_t calc_temperature(uint32_t temp_adc, struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to calculate the pressure value.
 *
 * @param[in] dev   :Structure instance of bme680_cfg.
 * @param[in] pres_adc  :Contains the pressure ADC value .
 *
 * @return uint32_t calculated pressure.
 */
static uint32_t calc_pressure(uint32_t pres_adc, const struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to calculate the humidity value.
 *
 * @param[in] dev   :Structure instance of bme680_cfg.
 * @param[in] hum_adc  :Contains the humidity ADC value.
 *
 * @return uint32_t calculated humidity.
 */
static uint32_t calc_humidity(uint16_t hum_adc, const struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to calculate the Gas Resistance value.
 *
 * @param[in] dev           :Structure instance of bme680_cfg.
 * @param[in] gas_res_adc   :Contains the Gas Resistance ADC value.
 * @param[in] gas_range     :Contains the range of gas values.
 *
 * @return uint32_t calculated gas resistance.
 */
static uint32_t calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range,
        const struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to calculate the Heat Resistance value.
 *
 * @param[in] dev   : Structure instance of bme680_cfg
 * @param[in] temp  : Contains the target temperature value.
 *
 * @return uint8_t calculated heater resistance.
 */
static uint8_t calc_heater_res(uint16_t temp, const struct bme680_cfg *dev);

#else
/*!
 * @brief This internal API is used to calculate the
 * temperature value value in float format
 *
 * @param[in] dev       :Structure instance of bme680_cfg.
 * @param[in] temp_adc  :Contains the temperature ADC value .
 *
 * @return Calculated temperature in float
 */
static float calc_temperature(uint32_t temp_adc, struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to calculate the
 * pressure value value in float format
 *
 * @param[in] dev       :Structure instance of bme680_cfg.
 * @param[in] pres_adc  :Contains the pressure ADC value .
 *
 * @return Calculated pressure in float.
 */
static float calc_pressure(uint32_t pres_adc, const struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to calculate the
 * humidity value value in float format
 *
 * @param[in] dev       :Structure instance of bme680_cfg.
 * @param[in] hum_adc   :Contains the humidity ADC value.
 *
 * @return Calculated humidity in float.
 */
static float calc_humidity(uint16_t hum_adc, const struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to calculate the
 * gas resistance value value in float format
 *
 * @param[in] dev           :Structure instance of bme680_cfg.
 * @param[in] gas_res_adc   :Contains the Gas Resistance ADC value.
 * @param[in] gas_range     :Contains the range of gas values.
 *
 * @return Calculated gas resistance in float.
 */
static float calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range,
        const struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to calculate the
 * heater resistance value in float format
 *
 * @param[in] temp  : Contains the target temperature value.
 * @param[in] dev   : Structure instance of bme680_cfg.
 *
 * @return Calculated heater resistance in float.
 */
static float calc_heater_res(uint16_t temp, const struct bme680_cfg *dev);

#endif

/*!
 * @brief This internal API is used to calculate the field data of sensor.
 *
 * @param[out] data :Structure instance to hold the data
 * @param[in] dev   :Structure instance of bme680_cfg.
 *
 *  @return int8_t result of the field data from sensor.
 */
static int8_t read_field_data(struct bme680_field_data *data,
                                struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to set the memory page
 * based on register address.
 *
 * The value of memory page
 *  value  | Description
 * --------|--------------
 *   0     | BME680_PAGE0_SPI
 *   1     | BME680_PAGE1_SPI
 *
 * @param[in] dev       :Structure instance of bme680_cfg.
 * @param[in] reg_addr  :Contains the register address array.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t set_mem_page(uint8_t reg_addr, struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to get the memory page based
 * on register address.
 *
 * The value of memory page
 *  value  | Description
 * --------|--------------
 *   0     | BME680_PAGE0_SPI
 *   1     | BME680_PAGE1_SPI
 *
 * @param[in] dev   :Structure instance of bme680_cfg.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t get_mem_page(struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to validate the device pointer for
 * null conditions.
 *
 * @param[in] dev   :Structure instance of bme680_cfg.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t null_ptr_check(const struct bme680_cfg *dev);

/*!
 * @brief This internal API is used to check the boundary
 * conditions.
 *
 * @param[in] value :pointer to the value.
 * @param[in] min   :minimum value.
 * @param[in] max   :maximum value.
 * @param[in] dev   :Structure instance of bme680_cfg.
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 */
static int8_t boundary_check(uint8_t *value, uint8_t min, uint8_t max,
                                struct bme680_cfg *dev);

/*!
 *@brief This API is the entry point.
 *It reads the chip-id and calibration data from the sensor.
 */
static int8_t
bme680_internal_init(struct bme680_cfg *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        /* Soft reset to restore it to default values*/
        rslt = bme680_soft_reset(dev);
        if (rslt == BME680_OK) {
            rslt = bme680_get_regs(BME680_CHIP_ID_ADDR, &dev->chip_id, 1, dev);
            if (rslt == BME680_OK) {
                if (dev->chip_id == BME680_CHIP_ID) {
                    /* Get the Calibration data */
                    rslt = get_calib_data(dev);
                } else {
                    rslt = BME680_E_DEV_NOT_FOUND;
                }
            }
        }
    }

    return rslt;
}

/*!
 * @brief This API reads the data from the given register address of the sensor.
 */
static int8_t
bme680_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint16_t len,
                        struct bme680_cfg *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        if (dev->intf == BME680_SPI_INTF) {
            /* Set the memory page */
            rslt = set_mem_page(reg_addr, dev);
            if (rslt == BME680_OK) {
                reg_addr = reg_addr | BME680_SPI_RD_MSK;
            }
        }
        dev->com_rslt = dev->read(dev->sensor, dev->dev_id,
                                    reg_addr, reg_data, len);
        if (dev->com_rslt != 0) {
            rslt = BME680_E_COM_FAIL;
        }
    }

    return rslt;
}

/*!
 * @brief This API writes the given data to the register address
 * of the sensor.
 */
static int8_t
bme680_set_regs(const uint8_t *reg_addr, const uint8_t *reg_data,
                        uint8_t len, struct bme680_cfg *dev)
{
    int8_t rslt;
    /* Length of the temporary buffer is 2*(length of register)*/
    uint8_t tmp_buff[BME680_TMP_BUFFER_LENGTH] = { 0 };
    uint16_t index;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        if ((len > 0) && (len < BME680_TMP_BUFFER_LENGTH / 2)) {
            /* Interleave the 2 arrays */
            for (index = 0; index < len; index++) {
                if (dev->intf == BME680_SPI_INTF) {
                    /* Set the memory page */
                    rslt = set_mem_page(reg_addr[index], dev);
                    tmp_buff[(2 * index)] = reg_addr[index] & BME680_SPI_WR_MSK;
                } else {
                    tmp_buff[(2 * index)] = reg_addr[index];
                }
                tmp_buff[(2 * index) + 1] = reg_data[index];
            }
            /* Write the interleaved array */
            if (rslt == BME680_OK) {
                dev->com_rslt = dev->write(dev->sensor, dev->dev_id,
                                    tmp_buff[0], &tmp_buff[1], (2 * len) - 1);
                if (dev->com_rslt != 0) {
                    rslt = BME680_E_COM_FAIL;
                }
            }
        } else {
            rslt = BME680_E_INVALID_LENGTH;
        }
    }

    return rslt;
}

/*!
 * @brief This API performs the soft reset of the sensor.
 */
static int8_t
bme680_soft_reset(struct bme680_cfg *dev)
{
    int8_t rslt;
    uint8_t reg_addr = BME680_SOFT_RESET_ADDR;
    /* 0xb6 is the soft reset command */
    uint8_t soft_rst_cmd = BME680_SOFT_RESET_CMD;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        if (dev->intf == BME680_SPI_INTF) {
            rslt = get_mem_page(dev);
        }

        /* Reset the device */
        if (rslt == BME680_OK) {
            rslt = bme680_set_regs(&reg_addr, &soft_rst_cmd, 1, dev);
            /* Wait for 5ms */
            dev->delay_ms(BME680_RESET_PERIOD);

            if (rslt == BME680_OK) {
                /* After reset get the memory page */
                if (dev->intf == BME680_SPI_INTF) {
                    rslt = get_mem_page(dev);
                }
            }
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to set the oversampling, filter and T,P,H, gas
 * selection settings in the sensor.
 */
static int8_t
bme680_set_sensor_settings(uint16_t desired_settings,
                                    struct bme680_cfg *dev)
{
    int8_t rslt;
    uint8_t reg_addr;
    uint8_t data = 0;
    uint8_t count = 0;
    uint8_t reg_array[BME680_REG_BUFFER_LENGTH] = { 0 };
    uint8_t data_array[BME680_REG_BUFFER_LENGTH] = { 0 };
    /* Save intended power mode */
    uint8_t intended_power_mode = dev->power_mode;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        if (desired_settings & BME680_GAS_MEAS_SEL) {
            rslt = set_gas_config(dev);
        }

        dev->power_mode = BME680_SLEEP_MODE;
        if (rslt == BME680_OK) {
            rslt = bme680_set_sensor_mode(dev);
        }

        /* Selecting the filter */
        if (desired_settings & BME680_FILTER_SEL) {
            rslt = boundary_check(&dev->tph_sett.filter, BME680_FILTER_SIZE_0,
                                    BME680_FILTER_SIZE_127, dev);
            reg_addr = BME680_CONF_ODR_FILT_ADDR;

            if (rslt == BME680_OK) {
                rslt = bme680_get_regs(reg_addr, &data, 1, dev);
            }

            if (desired_settings & BME680_FILTER_SEL) {
                data = BME680_SET_BITS(data, BME680_FILTER,
                                        dev->tph_sett.filter);
            }

            reg_array[count] = reg_addr; /* Append configuration */
            data_array[count] = data;
            count++;
        }

        /* Selecting heater control for the sensor */
        if (desired_settings & BME680_HCNTRL_SEL) {
            rslt = boundary_check(&dev->gas_sett.heatr_ctrl,
                                    BME680_ENABLE_HEATER, BME680_DISABLE_HEATER,
                                    dev);
            reg_addr = BME680_CONF_HEAT_CTRL_ADDR;

            if (rslt == BME680_OK) {
                rslt = bme680_get_regs(reg_addr, &data, 1, dev);
            }
            data = BME680_SET_BITS_POS_0(data, BME680_HCTRL,
                                            dev->gas_sett.heatr_ctrl);

            reg_array[count] = reg_addr; /* Append configuration */
            data_array[count] = data;
            count++;
        }

        /* Selecting heater T,P oversampling for the sensor */
        if (desired_settings & (BME680_OST_SEL | BME680_OSP_SEL)) {
            rslt = boundary_check(&dev->tph_sett.os_temp, BME680_OS_NONE,
                                    BME680_OS_16X, dev);
            reg_addr = BME680_CONF_T_P_MODE_ADDR;

            if (rslt == BME680_OK) {
                rslt = bme680_get_regs(reg_addr, &data, 1, dev);
            }

            if (desired_settings & BME680_OST_SEL) {
                data = BME680_SET_BITS(data, BME680_OST, dev->tph_sett.os_temp);
            }

            if (desired_settings & BME680_OSP_SEL) {
                data = BME680_SET_BITS(data, BME680_OSP, dev->tph_sett.os_pres);
            }

            reg_array[count] = reg_addr;
            data_array[count] = data;
            count++;
        }

        /* Selecting humidity oversampling for the sensor */
        if (desired_settings & BME680_OSH_SEL) {
            rslt = boundary_check(&dev->tph_sett.os_hum, BME680_OS_NONE,
                                    BME680_OS_16X, dev);
            reg_addr = BME680_CONF_OS_H_ADDR;

            if (rslt == BME680_OK) {
                rslt = bme680_get_regs(reg_addr, &data, 1, dev);
            }
            data = BME680_SET_BITS_POS_0(data, BME680_OSH,
                                        dev->tph_sett.os_hum);

            reg_array[count] = reg_addr; /* Append configuration */
            data_array[count] = data;
            count++;
        }

        /* Selecting the runGas and NB conversion settings for the sensor */
        if (desired_settings & (BME680_RUN_GAS_SEL | BME680_NBCONV_SEL)) {
            rslt = boundary_check(&dev->gas_sett.run_gas, BME680_RUN_GAS_DISABLE,
                BME680_RUN_GAS_ENABLE, dev);
            if (rslt == BME680_OK) {
                /* Validate boundary conditions */
                rslt = boundary_check(&dev->gas_sett.nb_conv, BME680_NBCONV_MIN,
                    BME680_NBCONV_MAX, dev);
            }

            reg_addr = BME680_CONF_ODR_RUN_GAS_NBC_ADDR;

            if (rslt == BME680_OK) {
                rslt = bme680_get_regs(reg_addr, &data, 1, dev);
            }

            if (desired_settings & BME680_RUN_GAS_SEL) {
                data = BME680_SET_BITS(data, BME680_RUN_GAS,
                                        dev->gas_sett.run_gas);
            }

            if (desired_settings & BME680_NBCONV_SEL) {
                data = BME680_SET_BITS_POS_0(data, BME680_NBCONV,
                                                dev->gas_sett.nb_conv);
            }

            reg_array[count] = reg_addr; /* Append configuration */
            data_array[count] = data;
            count++;
        }

        if (rslt == BME680_OK) {
            rslt = bme680_set_regs(reg_array, data_array, count, dev);
        }

        /* Restore previous intended power mode */
        dev->power_mode = intended_power_mode;
    }

    return rslt;
}

/*!
 * @brief This API is used to get the oversampling, filter and T,P,H, gas selection
 * settings in the sensor.
 */
#ifdef ENABLE_UNUSED_FUNCTIONS
static int8_t
bme680_get_sensor_settings(uint16_t desired_settings, struct bme680_cfg *dev)
{
    int8_t rslt;
    /* starting address of the register array for burst read*/
    uint8_t reg_addr = BME680_CONF_HEAT_CTRL_ADDR;
    uint8_t data_array[BME680_REG_BUFFER_LENGTH] = { 0 };

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        rslt = bme680_get_regs(reg_addr, data_array,
                                BME680_REG_BUFFER_LENGTH, dev);

        if (rslt == BME680_OK) {
            if (desired_settings & BME680_GAS_MEAS_SEL) {
                rslt = get_gas_config(dev);
            }

            /* get the T,P,H ,Filter,ODR settings here */
            if (desired_settings & BME680_FILTER_SEL) {
                dev->tph_sett.filter =
                    BME680_GET_BITS(data_array[BME680_REG_FILTER_INDEX],
                                    BME680_FILTER);
            }

            if (desired_settings & (BME680_OST_SEL | BME680_OSP_SEL)) {
                dev->tph_sett.os_temp =
                    BME680_GET_BITS(data_array[BME680_REG_TEMP_INDEX],
                                    BME680_OST);
                dev->tph_sett.os_pres =
                    BME680_GET_BITS(data_array[BME680_REG_PRES_INDEX],
                                    BME680_OSP);
            }

            if (desired_settings & BME680_OSH_SEL) {
                dev->tph_sett.os_hum =
                    BME680_GET_BITS_POS_0(data_array[BME680_REG_HUM_INDEX],
                                            BME680_OSH);
            }

            /* get the gas related settings */
            if (desired_settings & BME680_HCNTRL_SEL) {
                dev->gas_sett.heatr_ctrl =
                    BME680_GET_BITS_POS_0(data_array[BME680_REG_HCTRL_INDEX],
                                            BME680_HCTRL);
            }

            if (desired_settings & (BME680_RUN_GAS_SEL | BME680_NBCONV_SEL)) {
                dev->gas_sett.nb_conv =
                    BME680_GET_BITS_POS_0(data_array[BME680_REG_NBCONV_INDEX],
                                            BME680_NBCONV);
                dev->gas_sett.run_gas =
                    BME680_GET_BITS(data_array[BME680_REG_RUN_GAS_INDEX],
                                        BME680_RUN_GAS);
            }
        }
    } else {
        rslt = BME680_E_NULL_PTR;
    }

    return rslt;
}
#endif

/*!
 * @brief This API is used to set the power mode of the sensor.
 */
static int8_t
bme680_set_sensor_mode(struct bme680_cfg *dev)
{
    int8_t rslt;
    uint8_t tmp_pow_mode;
    uint8_t pow_mode = 0;
    uint8_t reg_addr = BME680_CONF_T_P_MODE_ADDR;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        /* Call repeatedly until in sleep */
        do {
            rslt = bme680_get_regs(BME680_CONF_T_P_MODE_ADDR,
                                    &tmp_pow_mode, 1, dev);
            if (rslt == BME680_OK) {
                /* Put to sleep before changing mode */
                pow_mode = (tmp_pow_mode & BME680_MODE_MSK);

                if (pow_mode != BME680_SLEEP_MODE) {
                    /* Set to sleep */
                    tmp_pow_mode = tmp_pow_mode & (~BME680_MODE_MSK);
                    rslt = bme680_set_regs(&reg_addr, &tmp_pow_mode, 1, dev);
                    dev->delay_ms(BME680_POLL_PERIOD_MS);
                }
            }
        } while (pow_mode != BME680_SLEEP_MODE);

        /* Already in sleep */
        if (dev->power_mode != BME680_SLEEP_MODE) {
            tmp_pow_mode = (tmp_pow_mode & ~BME680_MODE_MSK) |
                                (dev->power_mode & BME680_MODE_MSK);
            if (rslt == BME680_OK) {
                rslt = bme680_set_regs(&reg_addr, &tmp_pow_mode, 1, dev);
            }
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to get the power mode of the sensor.
 */
static int8_t
bme680_get_sensor_mode(struct bme680_cfg *dev)
{
    int8_t rslt;
    uint8_t mode;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        rslt = bme680_get_regs(BME680_CONF_T_P_MODE_ADDR, &mode, 1, dev);
        /* Masking the other register bit info*/
        dev->power_mode = mode & BME680_MODE_MSK;
    }

    return rslt;
}

/*!
 * @brief This API is used to set the profile duration of the sensor.
 */
#ifdef ENABLE_UNUSED_FUNCTIONS
static void
bme680_set_profile_dur(uint16_t duration, struct bme680_cfg *dev)
{
    uint32_t tph_dur; /* Calculate in us */
    uint32_t meas_cycles;
    uint8_t os_to_meas_cycles[6] = {0, 1, 2, 4, 8, 16};

    meas_cycles = os_to_meas_cycles[dev->tph_sett.os_temp];
    meas_cycles += os_to_meas_cycles[dev->tph_sett.os_pres];
    meas_cycles += os_to_meas_cycles[dev->tph_sett.os_hum];

    /* TPH measurement duration */
    tph_dur = meas_cycles * UINT32_C(1963);
    tph_dur += UINT32_C(477 * 4); /* TPH switching duration */
    tph_dur += UINT32_C(477 * 5); /* Gas measurement duration */
    tph_dur += UINT32_C(500); /* Get it to the closest whole number.*/
    tph_dur /= UINT32_C(1000); /* Convert to ms */

    tph_dur += UINT32_C(1); /* Wake up duration of 1ms */
    /* The remaining time should be used for heating */
    dev->gas_sett.heatr_dur = duration - (uint16_t) tph_dur;
}
#endif

/*!
 * @brief This API is used to get the profile duration of the sensor.
 */
static void
bme680_get_profile_dur(uint16_t *duration, const struct bme680_cfg *dev)
{
    uint32_t tph_dur; /* Calculate in us */
    uint32_t meas_cycles;
    uint8_t os_to_meas_cycles[6] = {0, 1, 2, 4, 8, 16};

    meas_cycles = os_to_meas_cycles[dev->tph_sett.os_temp];
    meas_cycles += os_to_meas_cycles[dev->tph_sett.os_pres];
    meas_cycles += os_to_meas_cycles[dev->tph_sett.os_hum];

    /* TPH measurement duration */
    tph_dur = meas_cycles * UINT32_C(1963);
    tph_dur += UINT32_C(477 * 4); /* TPH switching duration */
    tph_dur += UINT32_C(477 * 5); /* Gas measurement duration */
    tph_dur += UINT32_C(500); /* Get it to the closest whole number.*/
    tph_dur /= UINT32_C(1000); /* Convert to ms */

    tph_dur += UINT32_C(1); /* Wake up duration of 1ms */

    *duration = (uint16_t) tph_dur;

    /* Get the gas duration only when the run gas is enabled */
    if (dev->gas_sett.run_gas) {
        /* The remaining time should be used for heating */
        *duration += dev->gas_sett.heatr_dur;
    }
}

/*!
 * @brief This API reads the pressure, temperature and humidity and gas data
 * from the sensor, compensates the data and store it in the bme680_data
 * structure instance passed by the user.
 */
static int8_t
bme680_get_sensor_data(struct bme680_field_data *data,
                                struct bme680_cfg *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        /* Reading the sensor data in forced mode only */
        rslt = read_field_data(data, dev);
        if (rslt == BME680_OK) {
            if (data->status & BME680_NEW_DATA_MSK) {
                dev->new_fields = 1;
            } else {
                dev->new_fields = 0;
            }
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to read the calibrated data from the sensor.
 */
static int8_t
get_calib_data(struct bme680_cfg *dev)
{
    int8_t rslt;
    uint8_t coeff_array[BME680_COEFF_SIZE] = { 0 };
    uint8_t temp_var = 0; /* Temporary variable */

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        rslt = bme680_get_regs(BME680_COEFF_ADDR1, coeff_array,
            BME680_COEFF_ADDR1_LEN, dev);
        /* Append the second half in the same array */
        if (rslt == BME680_OK) {
            rslt = bme680_get_regs(BME680_COEFF_ADDR2,
                &coeff_array[BME680_COEFF_ADDR1_LEN], BME680_COEFF_ADDR2_LEN,
                dev);
        }

        /* Temperature related coefficients */
        dev->calib.par_t1 =
            (uint16_t) (BME680_CONCAT_BYTES(coeff_array[BME680_T1_MSB_REG],
            coeff_array[BME680_T1_LSB_REG]));
        dev->calib.par_t2 =
            (int16_t) (BME680_CONCAT_BYTES(coeff_array[BME680_T2_MSB_REG],
            coeff_array[BME680_T2_LSB_REG]));
        dev->calib.par_t3 = (int8_t) (coeff_array[BME680_T3_REG]);

        /* Pressure related coefficients */
        dev->calib.par_p1 =
            (uint16_t) (BME680_CONCAT_BYTES(coeff_array[BME680_P1_MSB_REG],
            coeff_array[BME680_P1_LSB_REG]));
        dev->calib.par_p2 =
            (int16_t) (BME680_CONCAT_BYTES(coeff_array[BME680_P2_MSB_REG],
            coeff_array[BME680_P2_LSB_REG]));
        dev->calib.par_p3 = (int8_t) coeff_array[BME680_P3_REG];
        dev->calib.par_p4 =
            (int16_t) (BME680_CONCAT_BYTES(coeff_array[BME680_P4_MSB_REG],
            coeff_array[BME680_P4_LSB_REG]));
        dev->calib.par_p5 =
            (int16_t) (BME680_CONCAT_BYTES(coeff_array[BME680_P5_MSB_REG],
            coeff_array[BME680_P5_LSB_REG]));
        dev->calib.par_p6 = (int8_t) (coeff_array[BME680_P6_REG]);
        dev->calib.par_p7 = (int8_t) (coeff_array[BME680_P7_REG]);
        dev->calib.par_p8 =
            (int16_t) (BME680_CONCAT_BYTES(coeff_array[BME680_P8_MSB_REG],
            coeff_array[BME680_P8_LSB_REG]));
        dev->calib.par_p9 =
            (int16_t) (BME680_CONCAT_BYTES(coeff_array[BME680_P9_MSB_REG],
            coeff_array[BME680_P9_LSB_REG]));
        dev->calib.par_p10 = (uint8_t) (coeff_array[BME680_P10_REG]);

        /* Humidity related coefficients */
        dev->calib.par_h1 =
            (uint16_t) (((uint16_t) coeff_array[BME680_H1_MSB_REG] <<
                    BME680_HUM_REG_SHIFT_VAL) |
                    (coeff_array[BME680_H1_LSB_REG] & BME680_BIT_H1_DATA_MSK));
        dev->calib.par_h2 =
            (uint16_t) (((uint16_t) coeff_array[BME680_H2_MSB_REG] <<
                    BME680_HUM_REG_SHIFT_VAL) |
                    ((coeff_array[BME680_H2_LSB_REG]) >> BME680_HUM_REG_SHIFT_VAL));
        dev->calib.par_h3 = (int8_t) coeff_array[BME680_H3_REG];
        dev->calib.par_h4 = (int8_t) coeff_array[BME680_H4_REG];
        dev->calib.par_h5 = (int8_t) coeff_array[BME680_H5_REG];
        dev->calib.par_h6 = (uint8_t) coeff_array[BME680_H6_REG];
        dev->calib.par_h7 = (int8_t) coeff_array[BME680_H7_REG];

        /* Gas heater related coefficients */
        dev->calib.par_gh1 = (int8_t) coeff_array[BME680_GH1_REG];
        dev->calib.par_gh2 =
            (int16_t) (BME680_CONCAT_BYTES(coeff_array[BME680_GH2_MSB_REG],
            coeff_array[BME680_GH2_LSB_REG]));
        dev->calib.par_gh3 = (int8_t) coeff_array[BME680_GH3_REG];

        /* Other coefficients */
        if (rslt == BME680_OK) {
            rslt = bme680_get_regs(BME680_ADDR_RES_HEAT_RANGE_ADDR,
                                    &temp_var, 1, dev);

            dev->calib.res_heat_range = ((temp_var & BME680_RHRANGE_MSK) / 16);
            if (rslt == BME680_OK) {
                rslt = bme680_get_regs(BME680_ADDR_RES_HEAT_VAL_ADDR,
                                        &temp_var, 1, dev);

                dev->calib.res_heat_val = (int8_t) temp_var;
                if (rslt == BME680_OK) {
                    rslt = bme680_get_regs(BME680_ADDR_RANGE_SW_ERR_ADDR,
                                            &temp_var, 1, dev);
                }
            }
        }
        dev->calib.range_sw_err =
            ((int8_t) temp_var & (int8_t) BME680_RSERROR_MSK) / 16;
    }

    return rslt;
}

/*!
 * @brief This internal API is used to set the gas configuration of the sensor.
 */
static int8_t
set_gas_config(struct bme680_cfg *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {

        uint8_t reg_addr[2] = {0};
        uint8_t reg_data[2] = {0};

        if (dev->power_mode == BME680_FORCED_MODE) {
            reg_addr[0] = BME680_RES_HEAT0_ADDR;
            reg_data[0] = calc_heater_res(dev->gas_sett.heatr_temp, dev);
            reg_addr[1] = BME680_GAS_WAIT0_ADDR;
            reg_data[1] = calc_heater_dur(dev->gas_sett.heatr_dur);
            dev->gas_sett.nb_conv = 0;
        } else {
            rslt = BME680_W_DEFINE_PWR_MODE;
        }
        if (rslt == BME680_OK) {
            rslt = bme680_set_regs(reg_addr, reg_data, 2, dev);
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to get the gas configuration of the sensor.
 * @note heatr_temp and heatr_dur values are currently register data
 * and not the actual values set
 */
#ifdef ENABLE_UNUSED_FUNCTIONS
static int8_t
get_gas_config(struct bme680_cfg *dev)
{
    int8_t rslt;
    /* starting address of the register array for burst read*/
    uint8_t reg_addr1 = BME680_ADDR_SENS_CONF_START;
    uint8_t reg_addr2 = BME680_ADDR_GAS_CONF_START;
    uint8_t reg_data = 0;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        if (BME680_SPI_INTF == dev->intf) {
            /* Memory page switch the SPI address*/
            rslt = set_mem_page(reg_addr1, dev);
        }

        if (rslt == BME680_OK) {
            rslt = bme680_get_regs(reg_addr1, &reg_data, 1, dev);
            if (rslt == BME680_OK) {
                dev->gas_sett.heatr_temp = reg_data;
                rslt = bme680_get_regs(reg_addr2, &reg_data, 1, dev);
                if (rslt == BME680_OK) {
                    /* Heating duration register value */
                    dev->gas_sett.heatr_dur = reg_data;
                }
            }
        }
    }

    return rslt;
}
#endif

#ifndef BME680_FLOAT_POINT_COMPENSATION

/*!
 * @brief This internal API is used to calculate the temperature value.
 */
static int16_t
calc_temperature(uint32_t temp_adc, struct bme680_cfg *dev)
{
    volatile int64_t var1;
    volatile int64_t var2;
    volatile int64_t var3;
    volatile int16_t calc_temp;

    var1 = ((int32_t) temp_adc >> 3) - ((int32_t) dev->calib.par_t1 << 1);
    var2 = (var1 * (int32_t) dev->calib.par_t2) >> 11;
    var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
    var3 = ((var3) * ((int32_t) dev->calib.par_t3 << 4)) >> 14;
    dev->calib.t_fine = (int32_t) (var2 + var3);
    calc_temp = (int16_t) (((dev->calib.t_fine * 5) + 128) >> 8);

    return calc_temp;
}

/*!
 * @brief This internal API is used to calculate the pressure value.
 */
static uint32_t
calc_pressure(uint32_t pres_adc, const struct bme680_cfg *dev)
{
    int32_t var1 = 0;
    int32_t var2 = 0;
    int32_t var3 = 0;
    int32_t pressure_comp = 0;

    var1 = (((int32_t)dev->calib.t_fine) >> 1) - 64000;
    var2 = ((((var1 >> 2) * (var1 >> 2)) >> 11) *
        (int32_t)dev->calib.par_p6) >> 2;
    var2 = var2 + ((var1 * (int32_t)dev->calib.par_p5) << 1);
    var2 = (var2 >> 2) + ((int32_t)dev->calib.par_p4 << 16);
    var1 = (((((var1 >> 2) * (var1 >> 2)) >> 13) *
        ((int32_t)dev->calib.par_p3 << 5)) >> 3) +
        (((int32_t)dev->calib.par_p2 * var1) >> 1);
    var1 = var1 >> 18;
    var1 = ((32768 + var1) * (int32_t)dev->calib.par_p1) >> 15;
    pressure_comp = 1048576 - pres_adc;
    pressure_comp = (int32_t)((pressure_comp - (var2 >> 12)) * ((uint32_t)3125));
    if (pressure_comp >= BME680_MAX_OVERFLOW_VAL) {
        pressure_comp = ((pressure_comp / (uint32_t)var1) << 1);
    } else {
        pressure_comp = ((pressure_comp << 1) / (uint32_t)var1);
    }
    var1 = ((int32_t)dev->calib.par_p9 * (int32_t)(((pressure_comp >> 3) *
        (pressure_comp >> 3)) >> 13)) >> 12;
    var2 = ((int32_t)(pressure_comp >> 2) *
        (int32_t)dev->calib.par_p8) >> 13;
    var3 = ((int32_t)(pressure_comp >> 8) * (int32_t)(pressure_comp >> 8) *
        (int32_t)(pressure_comp >> 8) *
        (int32_t)dev->calib.par_p10) >> 17;

    pressure_comp = (int32_t)(pressure_comp) + ((var1 + var2 + var3 +
        ((int32_t)dev->calib.par_p7 << 7)) >> 4);

    return (uint32_t)pressure_comp;

}

/*!
 * @brief This internal API is used to calculate the humidity value.
 */
static uint32_t
calc_humidity(uint16_t hum_adc, const struct bme680_cfg *dev)
{
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t var4;
    int32_t var5;
    int32_t var6;
    int32_t temp_scaled;
    int32_t calc_hum;

    temp_scaled = (((int32_t) dev->calib.t_fine * 5) + 128) >> 8;
    var1 = (int32_t) (hum_adc - ((int32_t) ((int32_t) dev->calib.par_h1 * 16)))
        - (((temp_scaled * (int32_t) dev->calib.par_h3) / ((int32_t) 100)) >> 1);
    var2 = ((int32_t) dev->calib.par_h2
        * (((temp_scaled * (int32_t) dev->calib.par_h4) / ((int32_t) 100))
            + (((temp_scaled * ((temp_scaled * (int32_t) dev->calib.par_h5) /
                ((int32_t) 100))) >> 6) / ((int32_t) 100)) +
                (int32_t) (1 << 14))) >> 10;
    var3 = var1 * var2;
    var4 = (int32_t) dev->calib.par_h6 << 7;
    var4 = ((var4) + ((temp_scaled * (int32_t) dev->calib.par_h7) /
            ((int32_t) 100))) >> 4;
    var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
    var6 = (var4 * var5) >> 1;
    calc_hum = (((var3 + var6) >> 10) * ((int32_t) 1000)) >> 12;

    if (calc_hum > 100000) {
        /* Cap at 100%rH */
        calc_hum = 100000;
    } else if (calc_hum < 0) {
        calc_hum = 0;
    }

    return (uint32_t) calc_hum;
}

/*!
 * @brief This internal API is used to calculate the Gas Resistance value.
 */
static uint32_t
calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range,
                                    const struct bme680_cfg *dev)
{
    int64_t var1;
    uint64_t var2;
    int64_t var3;
    uint32_t calc_gas_res;
    /**Look up table 1 for the possible gas range values */
    uint32_t lookupTable1[16] = { UINT32_C(2147483647), UINT32_C(2147483647),
        UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2147483647),
        UINT32_C(2126008810), UINT32_C(2147483647), UINT32_C(2130303777),
        UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2143188679),
        UINT32_C(2136746228), UINT32_C(2147483647), UINT32_C(2126008810),
        UINT32_C(2147483647), UINT32_C(2147483647) };
    /**Look up table 2 for the possible gas range values */
    uint32_t lookupTable2[16] = { UINT32_C(4096000000), UINT32_C(2048000000),
        UINT32_C(1024000000), UINT32_C(512000000), UINT32_C(255744255),
        UINT32_C(127110228), UINT32_C(64000000), UINT32_C(32258064),
        UINT32_C(16016016), UINT32_C(8000000), UINT32_C(4000000),
        UINT32_C(2000000), UINT32_C(1000000), UINT32_C(500000),
        UINT32_C(250000), UINT32_C(125000) };

    var1 = (int64_t) ((1340 + (5 * (int64_t) dev->calib.range_sw_err)) *
        ((int64_t) lookupTable1[gas_range])) >> 16;
    var2 = (((int64_t) ((int64_t) gas_res_adc << 15) - (int64_t) (16777216)) + var1);
    var3 = (((int64_t) lookupTable2[gas_range] * (int64_t) var1) >> 9);
    calc_gas_res = (uint32_t) ((var3 + ((int64_t) var2 >> 1)) / (int64_t) var2);

    return calc_gas_res;
}

/*!
 * @brief This internal API is used to calculate the Heat Resistance value.
 */
static uint8_t
calc_heater_res(uint16_t temp, const struct bme680_cfg *dev)
{
    uint8_t heatr_res;
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t var4;
    int32_t var5;
    int32_t heatr_res_x100;

    if (temp > 400) {
        /* Cap temperature */
        temp = 400;
    }

    var1 = (((int32_t) dev->amb_temp * dev->calib.par_gh3) / 1000) * 256;
    var2 = (dev->calib.par_gh1 + 784) *
        (((((dev->calib.par_gh2 + 154009) * temp * 5) / 100) + 3276800) / 10);
    var3 = var1 + (var2 / 2);
    var4 = (var3 / (dev->calib.res_heat_range + 4));
    var5 = (131 * dev->calib.res_heat_val) + 65536;
    heatr_res_x100 = (int32_t) (((var4 / var5) - 250) * 34);
    heatr_res = (uint8_t) ((heatr_res_x100 + 50) / 100);

    return heatr_res;
}

#else


/*!
 * @brief This internal API is used to calculate the
 * temperature value in float format
 */
static float
calc_temperature(uint32_t temp_adc, struct bme680_cfg *dev)
{
    float var1 = 0;
    float var2 = 0;
    float calc_temp = 0;

    /* calculate var1 data */
    var1  = ((((float)temp_adc / 16384.0f) -
            ((float)dev->calib.par_t1 / 1024.0f)) * ((float)dev->calib.par_t2));

    /* calculate var2 data */
    var2  = (((((float)temp_adc / 131072.0f) -
            ((float)dev->calib.par_t1 / 8192.0f)) *
            (((float)temp_adc / 131072.0f) -
            ((float)dev->calib.par_t1 / 8192.0f))) *
            ((float)dev->calib.par_t3 * 16.0f));

    /* t_fine value*/
    dev->calib.t_fine = (var1 + var2);

    /* compensated temperature data*/
    calc_temp  = ((dev->calib.t_fine) / 5120.0f);

    return calc_temp;
}

/*!
 * @brief This internal API is used to calculate the
 * pressure value in float format
 */
static float
calc_pressure(uint32_t pres_adc, const struct bme680_cfg *dev)
{
    float var1 = 0;
    float var2 = 0;
    float var3 = 0;
    float calc_pres = 0;

    var1 = (((float)dev->calib.t_fine / 2.0f) - 64000.0f);
    var2 = var1 * var1 * (((float)dev->calib.par_p6) / (131072.0f));
    var2 = var2 + (var1 * ((float)dev->calib.par_p5) * 2.0f);
    var2 = (var2 / 4.0f) + (((float)dev->calib.par_p4) * 65536.0f);
    var1 = (((((float)dev->calib.par_p3 * var1 * var1) / 16384.0f)
        + ((float)dev->calib.par_p2 * var1)) / 524288.0f);
    var1 = ((1.0f + (var1 / 32768.0f)) * ((float)dev->calib.par_p1));
    calc_pres = (1048576.0f - ((float)pres_adc));

    /* Avoid exception caused by division by zero */
    if ((int)var1 != 0) {
        calc_pres = (((calc_pres - (var2 / 4096.0f)) * 6250.0f) / var1);
        var1 = (((float)dev->calib.par_p9) * calc_pres * calc_pres) / 2147483648.0f;
        var2 = calc_pres * (((float)dev->calib.par_p8) / 32768.0f);
        var3 = ((calc_pres / 256.0f) * (calc_pres / 256.0f) * (calc_pres / 256.0f)
            * (dev->calib.par_p10 / 131072.0f));
        calc_pres = (calc_pres + (var1 + var2 + var3 +
                    ((float)dev->calib.par_p7 * 128.0f)) / 16.0f);
    } else {
        calc_pres = 0;
    }

    return calc_pres;
}

/*!
 * @brief This internal API is used to calculate the
 * humidity value in float format
 */
static float
calc_humidity(uint16_t hum_adc, const struct bme680_cfg *dev)
{
    float calc_hum = 0;
    float var1 = 0;
    float var2 = 0;
    float var3 = 0;
    float var4 = 0;
    float temp_comp;

    /* compensated temperature data*/
    temp_comp  = ((dev->calib.t_fine) / 5120.0f);

    var1 = (float)((float)hum_adc) - (((float)dev->calib.par_h1 * 16.0f) +
            (((float)dev->calib.par_h3 / 2.0f) * temp_comp));

    var2 = var1 * ((float)(((float) dev->calib.par_h2 / 262144.0f) *
            (1.0f + (((float)dev->calib.par_h4 / 16384.0f) * temp_comp) +
            (((float)dev->calib.par_h5 / 1048576.0f) * temp_comp * temp_comp))));

    var3 = (float) dev->calib.par_h6 / 16384.0f;

    var4 = (float) dev->calib.par_h7 / 2097152.0f;

    calc_hum = var2 + ((var3 + (var4 * temp_comp)) * var2 * var2);

    if (calc_hum > 100.0f) {
        calc_hum = 100.0f;
    } else if (calc_hum < 0.0f) {
        calc_hum = 0.0f;
    }

    return calc_hum;
}

/*!
 * @brief This internal API is used to calculate the
 * gas resistance value in float format
 */
static float
calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range,
                const struct bme680_cfg *dev)
{
    float calc_gas_res;
    float var1 = 0;
    float var2 = 0;
    float var3 = 0;

    const float lookup_k1_range[16] = {
    0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, -0.8,
    0.0, 0.0, -0.2, -0.5, 0.0, -1.0, 0.0, 0.0};
    const float lookup_k2_range[16] = {
    0.0, 0.0, 0.0, 0.0, 0.1, 0.7, 0.0, -0.8,
    -0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    var1 = (1340.0f + (5.0f * dev->calib.range_sw_err));
    var2 = (var1) * (1.0f + lookup_k1_range[gas_range]/100.0f);
    var3 = 1.0f + (lookup_k2_range[gas_range]/100.0f);

    calc_gas_res = 1.0f / (float)(var3 * (0.000000125f) *
                    (float)(1 << gas_range) *
                    (((((float)gas_res_adc) - 512.0f)/var2) + 1.0f));

    return calc_gas_res;
}

/*!
 * @brief This internal API is used to calculate the
 * heater resistance value in float format
 */
static float
calc_heater_res(uint16_t temp, const struct bme680_cfg *dev)
{
    float var1 = 0;
    float var2 = 0;
    float var3 = 0;
    float var4 = 0;
    float var5 = 0;
    float res_heat = 0;

    if (temp > 400) {
        /* Cap temperature */
        temp = 400;
    }

    var1 = (((float)dev->calib.par_gh1 / (16.0f)) + 49.0f);
    var2 = ((((float)dev->calib.par_gh2 / (32768.0f)) * (0.0005f)) + 0.00235f);
    var3 = ((float)dev->calib.par_gh3 / (1024.0f));
    var4 = (var1 * (1.0f + (var2 * (float)temp)));
    var5 = (var4 + (var3 * (float)dev->amb_temp));
    res_heat = (uint8_t)(3.4f * ((var5 * (4 / (4 + (float)dev->calib.res_heat_range)) *
        (1/(1 + ((float) dev->calib.res_heat_val * 0.002f)))) - 25));

    return res_heat;
}

#endif

/*!
 * @brief This internal API is used to calculate the Heat duration value.
 */
static uint8_t
calc_heater_dur(uint16_t dur)
{
    uint8_t factor = 0;
    uint8_t durval;

    if (dur >= 0xfc0) {
        durval = 0xff; /* Max duration*/
    } else {
        while (dur > 0x3F) {
            dur = dur / 4;
            factor += 1;
        }
        durval = (uint8_t) (dur + (factor * 64));
    }

    return durval;
}

/*!
 * @brief This internal API is used to calculate the field data of sensor.
 */
static int8_t
read_field_data(struct bme680_field_data *data, struct bme680_cfg *dev)
{
    int8_t rslt;
    uint8_t buff[BME680_FIELD_LENGTH] = { 0 };
    uint8_t gas_range;
    uint32_t adc_temp;
    uint32_t adc_pres;
    uint16_t adc_hum;
    uint16_t adc_gas_res;
    uint8_t tries = 10;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    do {
        if (rslt == BME680_OK) {
            rslt = bme680_get_regs(((uint8_t) (BME680_FIELD0_ADDR)), buff,
                    (uint16_t) BME680_FIELD_LENGTH, dev);

            data->status = buff[0] & BME680_NEW_DATA_MSK;
            data->gas_index = buff[0] & BME680_GAS_INDEX_MSK;
            data->meas_index = buff[1];

            /* read the raw data from the sensor */
            adc_pres = (uint32_t) (((uint32_t) buff[2] * 4096) |
                        ((uint32_t) buff[3] * 16) | ((uint32_t) buff[4] / 16));
            adc_temp = (uint32_t) (((uint32_t) buff[5] * 4096) |
                        ((uint32_t) buff[6] * 16) | ((uint32_t) buff[7] / 16));
            adc_hum = (uint16_t) (((uint32_t) buff[8] * 256) | (uint32_t) buff[9]);
            adc_gas_res = (uint16_t) ((uint32_t) buff[13] * 4 |
                            (((uint32_t) buff[14]) / 64));
            gas_range = buff[14] & BME680_GAS_RANGE_MSK;

            data->status |= buff[14] & BME680_GASM_VALID_MSK;
            data->status |= buff[14] & BME680_HEAT_STAB_MSK;

            if (data->status & BME680_NEW_DATA_MSK) {
                data->temperature = calc_temperature(adc_temp, dev);
                data->pressure = calc_pressure(adc_pres, dev);
                data->humidity = calc_humidity(adc_hum, dev);
                data->gas_resistance =
                    calc_gas_resistance(adc_gas_res, gas_range, dev);
                break;
            }
            /* Delay to poll the data */
            dev->delay_ms(BME680_POLL_PERIOD_MS);
        }
        tries--;
    } while (tries);

    if (!tries) {
        rslt = BME680_W_NO_NEW_DATA;
    }

    return rslt;
}

/*!
 * @brief This internal API is used to set the memory page
 * based on register address.
 */
static int8_t
set_mem_page(uint8_t reg_addr, struct bme680_cfg *dev)
{
    int8_t rslt;
    uint8_t reg;
    uint8_t mem_page;

    /* Check for null pointers in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        if (reg_addr > 0x7f) {
            mem_page = BME680_MEM_PAGE1;
        } else {
            mem_page = BME680_MEM_PAGE0;
        }

        if (mem_page != dev->mem_page) {
            dev->mem_page = mem_page;

            dev->com_rslt = dev->read(dev->sensor, dev->dev_id,
                BME680_MEM_PAGE_ADDR | BME680_SPI_RD_MSK, &reg, 1);
            if (dev->com_rslt != 0) {
                rslt = BME680_E_COM_FAIL;
            }

            if (rslt == BME680_OK) {
                reg = reg & (~BME680_MEM_PAGE_MSK);
                reg = reg | (dev->mem_page & BME680_MEM_PAGE_MSK);

                dev->com_rslt = dev->write(dev->sensor, dev->dev_id,
                    BME680_MEM_PAGE_ADDR & BME680_SPI_WR_MSK, &reg, 1);
                if (dev->com_rslt != 0) {
                    rslt = BME680_E_COM_FAIL;
                }
            }
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to get the memory page based on register address.
 */
static int8_t
get_mem_page(struct bme680_cfg *dev)
{
    int8_t rslt;
    uint8_t reg;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME680_OK) {
        dev->com_rslt = dev->read(dev->sensor, dev->dev_id,
            BME680_MEM_PAGE_ADDR | BME680_SPI_RD_MSK, &reg, 1);
        if (dev->com_rslt != 0) {
            rslt = BME680_E_COM_FAIL;
        } else {
            dev->mem_page = reg & BME680_MEM_PAGE_MSK;
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to validate the boundary
 * conditions.
 */
static int8_t
boundary_check(uint8_t *value, uint8_t min, uint8_t max, struct bme680_cfg *dev)
{
    int8_t rslt = BME680_OK;

    if (value != NULL) {
        /* Check if value is below minimum value */
        if (*value < min) {
            /* Auto correct the invalid value to minimum value */
            *value = min;
            dev->info_msg |= BME680_I_MIN_CORRECTION;
        }
        /* Check if value is above maximum value */
        if (*value > max) {
            /* Auto correct the invalid value to maximum value */
            *value = max;
            dev->info_msg |= BME680_I_MAX_CORRECTION;
        }
    } else {
        rslt = BME680_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This internal API is used to validate the device structure pointer for
 * null conditions.
 */
static int8_t
null_ptr_check(const struct bme680_cfg *dev)
{
    int8_t rslt;

    if ((dev == NULL) || (dev->read == NULL) || (dev->write == NULL) ||
        (dev->delay_ms == NULL)) {
        /* Device structure pointer is not valid */
        rslt = BME680_E_NULL_PTR;
    } else {
        /* Device structure is fine */
        rslt = BME680_OK;
    }

    return rslt;
}

static void
bme680_delay_ms(uint32_t period)
{
    uint32_t ticks;

    os_time_ms_to_ticks(period, &ticks);
    os_time_delay(ticks);
}

static int8_t
bme680_spi_read(struct sensor *sensor, uint8_t dev_id, uint8_t reg_addr,
                uint8_t *reg_data, uint16_t len)
{
    struct sensor_itf *interface = SENSOR_GET_ITF(sensor);

    hal_gpio_write(interface->si_cs_pin, 0);
    hal_spi_tx_val(interface->si_num, reg_addr | 0x80);
    /* Use reg_data for the txbuf as well.
    The contents don't matter since we're reading */
    hal_spi_txrx(interface->si_num, reg_data, reg_data, len);
    hal_gpio_write(interface->si_cs_pin, 1);

    return 0;
}

static int8_t
bme680_spi_write(struct sensor *sensor, uint8_t dev_id, uint8_t reg_addr,
                    uint8_t *reg_data, uint16_t len)
{
    struct sensor_itf *interface = SENSOR_GET_ITF(sensor);

    hal_gpio_write(interface->si_cs_pin, 0);
    hal_spi_tx_val(interface->si_num, reg_addr);
    hal_spi_txrx(interface->si_num, reg_data, NULL, len);
    hal_gpio_write(interface->si_cs_pin, 1);

    return 0;
}

static int8_t
bme680_i2c_read(struct sensor *sensor, uint8_t dev_id, uint8_t reg_addr,
                uint8_t *reg_data, uint16_t len)
{
    int8_t rc = 0;
    struct sensor_itf *interface = SENSOR_GET_ITF(sensor);
    struct hal_i2c_master_data data;
    data.address = dev_id;

    /* Send the register to read from */
    data.buffer = &reg_addr;
    data.len = 1;
    rc = hal_i2c_master_write(interface->si_num, &data, OS_TICKS_PER_SEC, 0);
    if (rc != 0) {
        goto exit;
    }

    /* Now read the data */
    data.buffer = reg_data;
    data.len = len;
    rc = hal_i2c_master_read(interface->si_num, &data, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        goto exit;
    }

exit:
    return rc;
}

static int8_t
bme680_i2c_write(struct sensor *sensor, uint8_t dev_id, uint8_t reg_addr,
                    uint8_t *reg_data, uint16_t len)
{
    int8_t rc = 0;
    struct sensor_itf *interface = SENSOR_GET_ITF(sensor);
    struct hal_i2c_master_data data;
    uint8_t buf[len + 1];

    /* copy the data into a single buffer */
    buf[0] = reg_addr;
    memcpy(&buf[1], reg_data, len);

    data.address = dev_id;
    data.buffer = buf;
    data.len = len + 1;
    rc = hal_i2c_master_write(interface->si_num, &data, 100, 1);

    return rc;
}

/* Exports for the sensor API */
static int bme680_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int bme680_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static int
bme680_sensor_read(struct sensor *sensor, sensor_type_t type,
                    sensor_data_func_t data_func, void *data_arg,
                    uint32_t timeout)
{
    int rc = 0;
    struct bme680 *bme680 = (struct bme680 *)SENSOR_GET_DEVICE(sensor);
    struct bme680_cfg *cfg = &bme680->cfg;
    uint16_t meas_duration;
    struct bme680_field_data data;

#if MYNEWT_VAL(BME680_USE_MYNEWT_SENSOR_DATA_TYPES)
    struct sensor_humid_data humidity;
    struct sensor_press_data pressure;
    struct sensor_temp_data temperature;
#endif
    void *tp, *pp, *hp;

    /* Trigger the sensor to take a sample */
    cfg->power_mode = BME680_FORCED_MODE;
    rc = bme680_set_sensor_settings(cfg->required_settings, cfg);
    if (rc != 0) {
        goto err;
    }

    rc = bme680_set_sensor_mode(cfg);
    if (rc != 0) {
        goto err;
    }
    /* Wait until the measurement is complete */
    bme680_get_profile_dur(&meas_duration, cfg);
    bme680_delay_ms(meas_duration);

    /* Wait until the sensor goes back to sleep mode */
    bme680_get_sensor_mode(cfg);
    while (cfg->power_mode == BME680_FORCED_MODE) {
        bme680_delay_ms(5);
        bme680_get_sensor_mode(cfg);
    }

    rc = bme680_get_sensor_data(&data, cfg);
    if (rc != 0) {
        goto err;
    }

#if MYNEWT_VAL(BME680_USE_MYNEWT_SENSOR_DATA_TYPES)
#ifdef BME680_FLOAT_POINT_COMPENSATION
    temperature.std_temp = data.temperature;
    humidity.shd_humid = data.humidity;
#else
    temperature.std_temp = data.temperature / 100.0f;
    humidity.shd_humid = data.humidity / 1000.0f;
#endif
    pressure.spd_press = data.pressure;

    temperature.std_temp_is_valid = 1;
    pressure.spd_press_is_valid = 1;
    humidity.shd_humid_is_valid = 1;

    tp = &temperature;
    pp = &pressure;
    hp = &humidity;
#else
    /* Use the Bosch standard data types */
    tp = &data.temperature;
    pp = &data.pressure;
    hp = &data.humidity;
#endif

    if (data.status & BME680_NEW_DATA_MSK) {
        if (type & SENSOR_TYPE_TEMPERATURE) {
            rc = data_func(sensor, data_arg, tp, SENSOR_TYPE_TEMPERATURE);
            if (rc != 0) {
                goto err;
            }
        }

        if (type & SENSOR_TYPE_PRESSURE) {
            rc = data_func(sensor, data_arg, pp, SENSOR_TYPE_PRESSURE);
            if (rc != 0) {
                goto err;
            }
        }

        if (type & SENSOR_TYPE_RELATIVE_HUMIDITY) {
            rc = data_func(sensor, data_arg, hp, SENSOR_TYPE_RELATIVE_HUMIDITY);
            if (rc != 0) {
                goto err;
            }
        }

        if (type & BME680_SENSOR_TYPE_GAS_RESISTANCE) {
            if (data.status & BME680_GASM_VALID_MSK) {
                rc = data_func(sensor, data_arg, &data.gas_resistance,
                                BME680_SENSOR_TYPE_GAS_RESISTANCE);
                if (rc != 0) {
                    goto err;
                }
            }
        }
    } else {
        /* No new data */
        rc = SYS_EIO;
        goto err;
    }

err:
    return rc;
}

static int
bme680_sensor_get_config(struct sensor *sensor, sensor_type_t type,
                            struct sensor_cfg *cfg)
{
    int rc = 0;

    if (!(type &
        (SENSOR_TYPE_TEMPERATURE | SENSOR_TYPE_PRESSURE |
        SENSOR_TYPE_RELATIVE_HUMIDITY | BME680_SENSOR_TYPE_GAS_RESISTANCE))) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (type & BME680_SENSOR_TYPE_GAS_RESISTANCE) {
#ifdef BME680_FLOAT_POINT_COMPENSATION
    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;
#else
    cfg->sc_valtype = SENSOR_VALUE_TYPE_OPAQUE;
#endif
    } else {
#if defined(BME680_FLOAT_POINT_COMPENSATION) || \
            MYNEWT_VAL(BME680_USE_MYNEWT_SENSOR_DATA_TYPES)
        cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;
#else
        cfg->sc_valtype = SENSOR_VALUE_TYPE_OPAQUE;
#endif
    }

err:
    return (rc);
}

static const struct sensor_driver g_bme680_sensor_driver = {
    bme680_sensor_read,
    bme680_sensor_get_config
};

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this sensor
 * @param Pointer to a sensor_itf struct
 *
 * @return 0 on success, non-zero error on failure.
 */
int
bme680_init(struct os_dev *dev, void *arg)
{
    int rc = 0;
    struct bme680 *bme680 = (struct bme680 *)dev;
    struct sensor_itf *interface = (struct sensor_itf *)arg;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    rc = sensor_init(&bme680->sensor, dev);
    if (rc != 0) {
        goto err;
    }

    rc = sensor_set_driver(&bme680->sensor,
        SENSOR_TYPE_PRESSURE | SENSOR_TYPE_TEMPERATURE | SENSOR_TYPE_RELATIVE_HUMIDITY,
        (struct sensor_driver *)&g_bme680_sensor_driver);
    if (rc != 0) {
        goto err;
    }

    rc = sensor_set_interface(&bme680->sensor, interface);
    if (rc != 0) {
        goto err;
    }

    rc = sensor_mgr_register(&bme680->sensor);
    if (rc != 0) {
        goto err;
    }

err:
    return(rc);
}

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 * 
 * @return 0 on success, non-zero error on failure.
  */
int
bme680_config(struct bme680 *bme680, struct bme680_cfg *cfg)
{
    int rc;
    struct sensor_itf *interface = SENSOR_GET_ITF(&(bme680->sensor));

    memcpy(&bme680->cfg, cfg, sizeof(struct bme680_cfg));
    /* Populate the config struct interface data */
    if (interface->si_type == SENSOR_ITF_SPI) {
        bme680->cfg.intf = BME680_SPI_INTF;
        bme680->cfg.read = bme680_spi_read;
        bme680->cfg.write = bme680_spi_write;
        bme680->cfg.dev_id = interface->si_cs_pin;
    } else if (interface->si_type == SENSOR_ITF_I2C) {
        bme680->cfg.intf = BME680_I2C_INTF;
        bme680->cfg.read = bme680_i2c_read;
        bme680->cfg.write = bme680_i2c_write;
        bme680->cfg.dev_id = interface->si_addr;
    } else {
        /* Invalid interface */
        rc = SYS_EINVAL;
        goto err;
    }
    bme680->cfg.delay_ms = bme680_delay_ms;
    bme680->cfg.sensor = &bme680->sensor;
    sensor_set_type_mask(&bme680->sensor, bme680->cfg.s_mask);

    rc = bme680_internal_init(&bme680->cfg);
    if (rc != 0) {
        goto err;
    }

err:
    return rc;
}
