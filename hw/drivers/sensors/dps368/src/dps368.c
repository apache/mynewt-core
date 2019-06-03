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

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <syscfg/syscfg.h>
#include <math.h>

#include "sensor/sensor.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"


#include "dps368/dps368.h"
#include "dps368_priv.h"
#include <stats/stats.h>

/* Define stat names for querying */
STATS_NAME_START(dps368_stat_section)
    STATS_NAME(dps368_stat_section, read_errors)
    STATS_NAME(dps368_stat_section, write_errors)
STATS_NAME_END(dps368_stat_section)

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
static struct hal_spi_settings spi_dps368_settings_s = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};
#endif

/* Exports for the sensor API */
static int dps368_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int dps368_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);
static int dps368_sensor_set_config(struct sensor *, void *);

static const struct sensor_driver dps368_sensor_driver_s = {
    .sd_read = dps368_sensor_read,
    .sd_get_config = dps368_sensor_get_config,
    .sd_set_config = dps368_sensor_set_config,
    .sd_reset = dps368_soft_reset,
};



/*Local helper functions implementations*/
/**
 * Verify sensor on bus
 *
 * @param The sensor interface
 * @param ptr to indicate if sensor hw is ready
 * @return 0 on success, non-zero on failure
 */
static int
dps368_verify_sensor(struct sensor_itf *itf, uint8_t *sensor_exist)
{
    int rc;
    uint8_t hwid;

    rc = dps368_get_hwid(itf, &hwid);

    if (rc) {
        *sensor_exist = 0;
        return rc;
    }

    if (hwid == IFX_DPS368_DEV_PROD_REVID) {
        *sensor_exist = 1;
    } else {
        *sensor_exist = 0;
    }

    return 0;

}

/**
 * Check status to see if the sensor is ready finishing self init after boot/soft-reset
 * typical time is 40 ms after reboot/soft-reset
 * access/use sensor after this
 * @param The sensor interface
 * @param ptr to indicate if sensor hw is ready after initialization
 * @return 0 on success, non-zero on failure
 */
static int
dps368_is_init_complete(struct sensor_itf *itf, uint8_t *ready)
{
    uint8_t status;
    int rc;

    rc = dps368_read_regs(itf, IFX_DPS368_MEAS_CFG_REG_ADDR, &status,
            IFX_DPS368_MEAS_CFG_REG_LEN);

    if (rc) {
        return rc;
    }

    if (status & IFX_DPS368_MEAS_CFG_REG_SEN_RDY_VAL) {
        *ready = 1;
    } else {
        *ready = 0;
    }

    return 0;

}

/**
 * Check status to see if the sensor is ready trim
 * typical time is 12ms after reboot/soft-reset
 * read calib data after this
 * @param The sensor interface
 * @param ptr to indicate if sensor hw is ready after initialization
 * @return 0 on success, non-zero on failure
 */
static int
dps368_is_trim_complete(struct sensor_itf *itf, uint8_t *ready)
{
    uint8_t status;
    int rc;

    rc = dps368_read_regs(itf, IFX_DPS368_MEAS_CFG_REG_ADDR, &status,
            IFX_DPS368_MEAS_CFG_REG_LEN);

    if (rc) {
        *ready = 0;
        return rc;
    }

    if (status & IFX_DPS368_MEAS_CFG_REG_COEF_RDY_VAL) {
        *ready = 1;
    } else {
        *ready = 0;
    }

    return 0;
}

/**
 * Prepare calibration coefficients once sensor is ready
 *
 * @param The sensor interface
 * @param ptr read calib reg data
 * @return 0 on success, non-zero on failure
 */
static int
dps368_prepare_calib_coeff(struct sensor_itf *itf,
        dps3xx_cal_coeff_regs_s *coeffs, dps3xx_temperature_src_e *src_t)
{
     uint8_t read_buffer[IFX_DPS368_COEF_LEN] = {0};
     int rc;

     rc = dps368_read_regs(itf, IFX_DPS368_COEF_REG_ADDR, read_buffer,
             IFX_DPS368_COEF_LEN);

     if (rc) {
         return rc;
     }

     coeffs->C0 = (read_buffer[0] << 4) + ((read_buffer[1] >>4) & 0x0F);
     if(coeffs->C0 > POW_2_11_MINUS_1)
         coeffs->C0 = coeffs->C0 - POW_2_12;

     coeffs->C1 = (read_buffer[2] + ((read_buffer[1] & 0x0F)<<8));
     if(coeffs->C1 > POW_2_11_MINUS_1)
         coeffs->C1 = coeffs->C1 - POW_2_12;

     coeffs->C00 = ((read_buffer[4]<<4) + (read_buffer[3]<<12)) + ((read_buffer[5]>>4) & 0x0F);
     if(coeffs->C00 > POW_2_19_MINUS_1)
         coeffs->C00 = coeffs->C00 -POW_2_20;

     coeffs->C10 = ((read_buffer[5] & 0x0F)<<16) + read_buffer[7] + (read_buffer[6]<<8);
     if(coeffs->C10 > POW_2_19_MINUS_1)
         coeffs->C10 = coeffs->C10 - POW_2_20;

     coeffs->C01 = (read_buffer[9] + (read_buffer[8]<<8));
     if(coeffs->C01 > ((POW_2_15_MINUS_1)))
         coeffs->C01 = coeffs->C01 - POW_2_16;

     coeffs->C11 = (read_buffer[11] + (read_buffer[10]<<8));
     if(coeffs->C11 > ((POW_2_15_MINUS_1)))
         coeffs->C11 = coeffs->C11 - POW_2_16;

     coeffs->C20 = (read_buffer[13] + (read_buffer[12]<<8));
     if(coeffs->C20 > ((POW_2_15_MINUS_1)))
         coeffs->C20 = coeffs->C20 - POW_2_16;

     coeffs->C21 = (read_buffer[15] + (read_buffer[14]<<8));
     if(coeffs->C21 > ((POW_2_15_MINUS_1)))
         coeffs->C21 = coeffs->C21 - POW_2_16;

     coeffs->C30 = (read_buffer[17] + (read_buffer[16]<<8));
     if(coeffs->C30 > ((POW_2_15_MINUS_1)))
         coeffs->C30 = coeffs->C30 - POW_2_16;

     memset(read_buffer, 0, sizeof(IFX_DPS368_COEF_LEN));

     rc = dps368_read_regs(itf, IFX_DPS368_TMP_COEF_SRCE_REG_ADDR, read_buffer,
             IFX_DPS368_TMP_CFG_REG_LEN);

     if (rc) {
         return rc;
     }

    if ((read_buffer[0] >> IFX_DPS368_TMP_COEF_SRCE_REG_POS) & 1) {
        *src_t = TMP_EXT_MEMS;
    } else {
        *src_t = TMP_EXT_ASIC;
    }

     return 0;
}

/**
 * Choose appropriate scalling factor for given OSR
 *
 * @param selected osr
 * @return chosen scaling coefficient for given osr
 */
static dps3xx_scaling_coeffs_e
dps368_get_scaling_coef (dps3xx_osr_e osr)
{
    dps3xx_scaling_coeffs_e scaling_coeff;

    switch (osr){

    case OSR_1:
        scaling_coeff = OSR_SF_1;
        break;
    case OSR_2:
        scaling_coeff = OSR_SF_2;
        break;
    case OSR_4:
        scaling_coeff = OSR_SF_4;
        break;
    case OSR_8:
        scaling_coeff = OSR_SF_8;
        break;
    case OSR_16:
        scaling_coeff = OSR_SF_16;
        break;
    case OSR_32:
        scaling_coeff = OSR_SF_32;
        break;
    case OSR_64:
        scaling_coeff = OSR_SF_64;
        break;
    case OSR_128:
        scaling_coeff = OSR_SF_128;
        break;
    default:
        scaling_coeff = OSR_SF_1;
        break;
    }

    return scaling_coeff;
}

/**
 * post init sequence
 *
 * @param ptr sensor interface
 * @return 0 on success else non 0 val
 */
static int
dps368_set_oem_parameters(struct sensor_itf *itf)
{
    int rc;

    if ((rc = dps368_write_reg(itf, (uint8_t) 0x0E, (uint8_t) 0xA5))) {
        goto done;
    }

    if ((rc = dps368_write_reg(itf, (uint8_t) 0x0F, (uint8_t) 0x96))) {
        goto done;
    }

    if ((rc = dps368_write_reg(itf, (uint8_t) 0x62, (uint8_t) 0x02))) {
        goto done;
    }

    if ((rc = dps368_write_reg(itf, (uint8_t) 0x0E, (uint8_t) 0x00))) {
        goto done;
    }

    if ((rc = dps368_write_reg(itf, (uint8_t) 0x0F, (uint8_t) 0x00))) {
        goto done;
    }

    DPS368_LOG(INFO,"DPS368:OEM Parameters are set\n");

done:
    return rc;

}

/**
 * Choose maximum possible OSR given ODR
 *
 * @param selected odr
 * @return chosen OSR for given odr
 */
static dps3xx_osr_e
dps368_get_best_osr (dps3xx_odr_e odr)
{
    dps3xx_osr_e chosen_osr;
    switch (odr)
        {
        case ODR_1:
            chosen_osr = OSR_128;
            break;
        case ODR_2:
            chosen_osr = OSR_128;
            break;
        case ODR_4:
            chosen_osr = OSR_128;
            break;
        case ODR_8:
            chosen_osr = OSR_64;
            break;
        case ODR_16:
            chosen_osr = OSR_32;
            break;
        case ODR_32:
            chosen_osr = OSR_16;
            break;
        case ODR_64:
            chosen_osr = OSR_8;
            break;
        case ODR_128:
            chosen_osr = OSR_2;
            break;
        default:
            chosen_osr = OSR_2;
            break;
        }

    return chosen_osr;

}

/**
 * Internal Function:sets sensor to specific predefine operating mode
 *
 * @param Instance of sensor specific driver state
 * @param Instance of enumerated constants defining predefined
*         modes of operation
 * @return 0 on success, non-zero on failure
 */
static int
dps368_set_mode(struct dps368 *dps368, dps3xx_operating_modes_e mode)
{
    int rc;

    uint8_t md = (uint8_t)mode;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(dps368->sensor));

    if (dps368->mode == mode) {
        return 0;
        DPS368_LOG(INFO, "Sensor is already in requested mode\n");
    }


    rc = dps368_write_reg(itf, IFX_DPS368_MEAS_CFG_REG_ADDR, md);

    if (rc) {
        STATS_INC(dps368->stats, write_errors);
        return rc;
    }

    dps368->mode = mode;

    dps368->validated = 0;

    /*wait till config is complete*/
    os_time_delay((OS_TICKS_PER_SEC * IFX_DPS368_CONFIG_TIME_MS)/1000+1);

    return 0;

}

/**
* Configures the sensor to operate in given
* oversampling and streaming rates for both temperature and pressure
*
* @param Instance of sensor specific driver state
* @param Enumerated constants defining oversampling rate for
*        temperature measurements
* @param Enumerated constants defining oversampling rate for
*        pressure measurements
* @param Enumerated constants defining streaming rate (ODR) for
*        temperature measurements
* @param Enumerated constants defining streaming rate (ODR) for
*        pressure measurements
* @return 0 on success, non-zero on failure
*/
static int
dps368_reconfig(struct dps368 *dps368, dps3xx_osr_e osr_t,dps3xx_osr_e osr_p,
            dps3xx_odr_e odr_t,dps3xx_odr_e odr_p)
{
    int rc;

    struct sensor_itf *itf;

    uint8_t config_val_tmp = 0;
    uint8_t config_val_prs = 0;
    uint8_t config_val;


    dps3xx_operating_modes_e current_mode = dps368->mode;

    rc = dps368_set_mode(dps368, DPS3xx_MODE_IDLE);

    if (rc) {
        return rc;
    }

    itf = SENSOR_GET_ITF(&(dps368->sensor));

    /*Prepare a configuration word for TMP_CFG register*/
    config_val_tmp = (uint8_t) dps368->temp_src;

    /*First Set the TMP_RATE[2:0] -> 6:4 */
    config_val_tmp |= ((uint8_t)odr_t);

    /*Set the TMP_PRC[3:0] -> 2:0 */
    config_val_tmp |= ((uint8_t)osr_t);

    /*Prepare a configuration word for PRS_CFG register*/
    config_val_prs = (uint8_t) 0x00;
    /*First Set the PM_RATE[2:0] -> 6:4 */
    config_val_prs |= ((uint8_t)odr_p);
    /*Set the PM_PRC[3:0] -> 3:0 */
    config_val_prs |= ((uint8_t)osr_p);

    /*write register to set temperature measurement and oversampling rate.*/
    rc = dps368_write_reg(itf, IFX_DPS368_TMP_CFG_REG_ADDR, config_val_tmp);

    if (rc) {
        goto write_err;
    }

    rc = dps368_write_reg(itf, IFX_DPS368_PRS_CFG_REG_ADDR, config_val_prs);

    if (rc) {
        goto write_err;
    }


    config_val = dps368->cfg_word;

    /*If oversampling rate for temperature is greater than 8 times, then set TMP_SHIFT bit in CFG_REG */
    if ((uint8_t) osr_t > (uint8_t) OSR_8) {
        config_val |= (uint8_t) IFX_DPS368_CFG_TMP_SHIFT_EN_SET_VAL;
    }

    /*If oversampling rate for pressure is greater than 8 times, then set P_SHIFT bit in CFG_REG */
    if ((uint8_t) osr_p > (uint8_t) OSR_8) {
        config_val |= (uint8_t) IFX_DPS368_CFG_PRS_SHIFT_EN_SET_VAL;
    }

    rc = dps368_write_reg(itf,IFX_DPS368_CFG_REG_ADDR,config_val);

    if (rc){
        goto write_err;
    }

    /*Update state accordingly with proper scaling factors based on oversampling rates*/
    dps368->osr_scale_t = dps368_get_scaling_coef(osr_t);
    dps368->osr_scale_p =  dps368_get_scaling_coef(osr_p);

    dps368->validated = 0;

    /*restore mode*/
    rc = dps368_set_mode(dps368, current_mode);

    if (rc) {
        return rc;
    }
    return 0;
write_err:
    STATS_INC(dps368->stats, write_errors);
    return rc;
}

/**
 * Check and set the validity of sample in driver state
 *
 * @param  Instance of specific driver state
 * @return  0 on success, non-zero on failure
 */
static int
dps368_validate_sample_accuracy(struct dps368 *dps368)
{

    int rc;
    uint8_t read_buffer;
    struct sensor_itf *itf;
    uint8_t CHEK_MSK = 0;
    uint8_t CHEK_WORD =0;

    itf = SENSOR_GET_ITF(&(dps368->sensor));

    if ((rc = dps368_read_regs(itf, IFX_DPS368_MEAS_CFG_REG_ADDR, &read_buffer,
            IFX_DPS368_MEAS_CFG_REG_LEN))) {
        return rc;
    }

    if (DPS3xx_MODE_BACKGROUND_ALL == dps368->mode ||
            DPS3xx_MODE_BACKGROUND_PRESSURE == dps368->mode ||
            DPS3xx_MODE_COMMAND_PRESSURE == dps368->mode) {
        CHEK_MSK = 0b00110000;
        CHEK_WORD = 0x30; //BOTH P And T should be ready
    }
    else if (DPS3xx_MODE_BACKGROUND_TEMPERATURE == dps368->mode ||
                DPS3xx_MODE_COMMAND_TEMPERATURE == dps368->mode) {
        CHEK_MSK = 0b00100000;
        CHEK_WORD = 0x20; //ONLY T should be ready
    }


    if ((read_buffer & CHEK_MSK) == CHEK_WORD) {
        dps368->validated = 1;

    } else {
        dps368->validated = 0;

    }

    return 0;
}

/**
 * Gets compensated pressure in hecto Pascal
 *
 * @param Instance of sensor specific driver state
 * @param ptr computed and temperature compensated pressure in Pa
 *
 * @return 0 on success, and non-zero error code on failure
 */
static int
dps368_get_pressure_Pa(struct dps368 *dps368, float *pressurePa)
{
    int rc;
    uint8_t read_buffer[IFX_DPS368_PSR_TMP_READ_LEN] = {0};
    float  temp_raw;
    float  temp_scaled;
    float  press_scaled;
    float  press_final;
    float  press_raw;

    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(dps368->sensor));

    if (dps368->validated != 1) {
        rc = dps368_validate_sample_accuracy(dps368);
        if (rc) {
            STATS_INC(dps368->stats, read_errors); 
            return rc;
        }
    }

    rc = dps368_read_regs(itf, IFX_DPS368_PSR_TMP_READ_REG_ADDR, read_buffer,
                          IFX_DPS368_PSR_TMP_READ_LEN);
    if (rc) {
        STATS_INC(dps368->stats, read_errors); 
        return rc;
    }

    press_raw = (read_buffer[2]) + (read_buffer[1]<<8) + (read_buffer[0] <<16);

    temp_raw  = (read_buffer[5]) + (read_buffer[4]<<8) + (read_buffer[3] <<16);


    if (temp_raw > POW_2_23_MINUS_1) {
        temp_raw = temp_raw - POW_2_24;
    }

    temp_scaled = (float)temp_raw / (float) (dps368->osr_scale_t);

    if (press_raw > POW_2_23_MINUS_1) {
        press_raw = press_raw - POW_2_24;
    }

    press_scaled = (float) press_raw / dps368->osr_scale_p;

    press_final = dps368->calib_coeffs.C00 +
            press_scaled *  (dps368->calib_coeffs.C10 + press_scaled *
                    (dps368->calib_coeffs.C20 + press_scaled * dps368->calib_coeffs.C30 )) +
                    temp_scaled * dps368->calib_coeffs.C01 +temp_scaled * press_scaled *
                    ( dps368->calib_coeffs.C11 + press_scaled * dps368->calib_coeffs.C21 );

    *pressurePa  = press_final;

    return 0;
}

/**
 * Gets temperature in degree celcius
 *
 * @param Instance of sensor specific driver
 * @param ptr computed temperature in degree celcius
 *
 * @return 0 on success, and non-zero error code on failure
 */
static int
dps368_get_temperature_degC(struct dps368 *dps368, float *tempdegC)
{
    int rc;
    uint8_t read_buffer[IFX_DPS368_PSR_TMP_READ_LEN -3] = {0};
    float  temp_raw;
    float  temp_scaled;
    float  temp_final;

    struct sensor_itf *itf;


    itf = SENSOR_GET_ITF(&dps368->sensor);


    if (dps368->validated != 1) {

        rc = dps368_validate_sample_accuracy(dps368);

        if (rc) {
            return rc;
        }
    }


    rc = dps368_read_regs(itf, IFX_DPS368_PSR_TMP_READ_REG_ADDR + 3,
            read_buffer,
            IFX_DPS368_PSR_TMP_READ_LEN - 3);

    if (rc) {
        STATS_INC(dps368->stats, read_errors);
        return rc;
    }

    temp_raw = (read_buffer[2]) + (read_buffer[1]<<8) + (read_buffer[0] <<16);

    if (temp_raw > POW_2_23_MINUS_1) {
        temp_raw = temp_raw - POW_2_24;
    }

    temp_scaled = (float)temp_raw / (float) (dps368->osr_scale_t);

    temp_final =  (dps368->calib_coeffs.C0 /2.0f) + dps368->calib_coeffs.C1 * temp_scaled ;

    *tempdegC = temp_final;

    return 0;

}


/*Public functions implementations*/
int
dps368_get_hwid(struct sensor_itf *itf, uint8_t *hwid)
{
    int rc;
    uint8_t tmp;

    rc = dps368_read_regs(itf, IFX_DPS368_PROD_REV_ID_REG_ADDR, &tmp,
            IFX_DPS368_PROD_REV_ID_LEN);

    if (rc) {
        hwid = 0;
        return rc;
    }

    *hwid = tmp;

    return 0;
}

int dps368_soft_reset(struct sensor *sensor)
{
    int rc;

    uint8_t ready;

    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(sensor);

    rc = dps368_write_reg(itf, IFX_DPS368_SOFT_RESET_REG_ADDR,
            IFX_DPS368_SOFT_RESET_REG_DATA);

    if (rc) {
        STATS_INC(((struct dps368 *)sensor)->stats, write_errors);
        return rc;
    }

    /*wait till sensor boots up back*/
    os_time_delay(((OS_TICKS_PER_SEC * IFX_DPS368_OFF_TO_IDLE_MS)/1000)+1);

    /*Check if sensor is back */
    if ((rc = dps368_is_init_complete(itf, &ready))) {
        STATS_INC(((struct dps368 *)sensor)->stats, read_errors);
        return rc;
    }

    if (ready == 0) {
        DPS368_LOG(INFO, "Sensor is not ready after reset\n");
    } else {
        DPS368_LOG(INFO, "Sensor is ready after reset\n");
    }

    /*perform post init sequence*/
    if ((rc = dps368_set_oem_parameters(itf))) {
        STATS_INC(((struct dps368 *)sensor)->stats, write_errors);
        return rc;
    }

    return 0;

}

static void
dps368_stats_int(struct os_dev *dev)
{
    int rc;
    /* Initialise the stats entry */
    rc = stats_init(STATS_HDR(((struct dps368 *)dev)->stats),
            STATS_SIZE_INIT_PARMS(((struct dps368 *)dev)->stats, STATS_SIZE_32),
            STATS_NAME_INIT_PARMS(dps368_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(((struct dps368 *)dev)->stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
}

int dps368_init(struct os_dev *dev, void *arg)
{
    struct dps368 *dps368;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        return SYS_ENODEV;
    }

    dps368 = (struct dps368 *) dev;

    sensor = &dps368->sensor;

    dps368->cfg.config_opt = DPS3xx_CONF_WITH_INIT_SEQUENCE;

    dps368_stats_int(dev);

    rc = sensor_init(sensor, dev);

    if (rc) {
        return rc;
    }

    DPS368_LOG(INFO,"DPS368init:sensor_init:OK\n");

    /* Add the pressure and temperature driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_PRESSURE |
            SENSOR_TYPE_TEMPERATURE,
            (struct sensor_driver *) &dps368_sensor_driver_s);

    if (rc) {
        return rc;
    }

    DPS368_LOG(INFO,"DPS368init:sensor_set_driver:OK\n");

    rc = sensor_set_interface(sensor, arg);

    if (rc) {
        return rc;
    }

    DPS368_LOG(INFO,"DPS368init:sensor_set_interface:OK\n");

    rc = sensor_mgr_register(sensor);

    if (rc) {
        return rc;
    }

    DPS368_LOG(INFO,"DPS368init:sensor_mgr_register:OK\n");

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    if (sensor->s_itf.si_type == SENSOR_ITF_SPI) {

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_dps368_settings_s);

        if (rc == EINVAL) {
            return rc;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);

        if (rc) {
            return rc;
        }

        rc = hal_gpio_init_out(sensor->s_itf.si_cs_pin, 1);

        if (rc) {
            return rc;
        }
    }
#endif

    return 0;
}

int dps368_config(struct dps368 *dps368, struct dps368_cfg_s *cfg)
{

    int rc;
    struct sensor_itf *itf;

    uint8_t sensor_exist;
    uint8_t ready;

    struct dps368_cfg_s updated_cfg = dps368->cfg;

    itf = SENSOR_GET_ITF(&(dps368->sensor));

    if (cfg->config_opt & DPS3xx_CONF_WITH_INIT_SEQUENCE) {

        updated_cfg.odr_t = cfg->odr_t;
        updated_cfg.odr_p = cfg->odr_p;
        updated_cfg.osr_t = cfg->osr_t;
        updated_cfg.osr_p = cfg->osr_p;
        updated_cfg.mode  = cfg->mode;

        if ((rc = dps368_verify_sensor(itf, &sensor_exist))) {
            STATS_INC(dps368->stats, read_errors); 
            return rc;
        }

        if (sensor_exist == 1) {
            DPS368_LOG(INFO, "DPS368:Found during config init sequence\n");
        }

        if ((rc = dps368_soft_reset(&dps368->sensor))) {
            return rc;
        }

        DPS368_LOG(INFO, "DPS368:Soft reset: OK\n");

        if ((rc = dps368_is_trim_complete(itf, &ready))) {
            STATS_INC(dps368->stats, read_errors); 
            return rc;
        }

        if (ready == 1) {

            if ((rc = dps368_prepare_calib_coeff(itf, &dps368->calib_coeffs,
                    &dps368->temp_src))) {
                STATS_INC(dps368->stats, read_errors); 
                return rc;
            }

            DPS368_LOG(INFO, "DPS368:Calibration data prepared\n");
        }

    } else if (cfg->config_opt & DPS3xx_RECONF_ALL) {
        updated_cfg.odr_t = cfg->odr_t;
        updated_cfg.odr_p = cfg->odr_p;
        updated_cfg.osr_t = cfg->osr_t;
        updated_cfg.osr_p = cfg->osr_p;
        updated_cfg.mode  = cfg->mode;
    } else if (cfg->config_opt & DPS3xx_RECONF_ODR_ONLY) {
        updated_cfg.odr_t = cfg->odr_t;
        updated_cfg.odr_p = cfg->odr_p;
    } else if (cfg->config_opt & DPS3xx_RECONF_ODR_WITH_BEST_OSR) {
        updated_cfg.odr_t = cfg->odr_t;
        updated_cfg.odr_p = cfg->odr_p;
        updated_cfg.osr_t = dps368_get_best_osr(cfg->odr_t);
        updated_cfg.osr_p = dps368_get_best_osr(cfg->odr_p);
    } else if (cfg->config_opt & DPS3xx_RECONF_OSR_ONLY) {
        updated_cfg.osr_t = cfg->osr_t;
        updated_cfg.osr_p = cfg->osr_p;
    } else if (cfg->config_opt & DPS3xx_RECONF_MODE_ONLY) {
        updated_cfg.mode = cfg->mode;
    }

    if ((rc = dps368_reconfig(dps368, updated_cfg.osr_t, updated_cfg.osr_p,
            updated_cfg.odr_t, updated_cfg.odr_p))) {
        return rc;
    }

    DPS368_LOG(INFO, "DPS368:Reconfig done\n");

    if ((rc = dps368_set_mode(dps368, updated_cfg.mode))) {
        return rc;
    }

    DPS368_LOG(INFO, "DPS368:Mode is set\n");

    if ((rc = sensor_set_type_mask(&(dps368->sensor), cfg->chosen_type))) {
        return rc;
    }

    return 0;
}


/*Local call-backs implementations*/
static int
dps368_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int rc = SYS_EINVAL;

    struct dps368 *dps368;
    dps368 = (struct dps368 *)SENSOR_GET_DEVICE(sensor);
    (void)timeout;

    if (dps368->mode == DPS3xx_MODE_IDLE) {

        DPS368_LOG(ERROR,
                "Could not stream as mode is inappropriate\n "
                "First set mode to Background or Command and then try again\n");
        return rc;
    }

    if (type & SENSOR_TYPE_PRESSURE) {
        struct sensor_press_data spd;
        rc = dps368_get_pressure_Pa(dps368, &spd.spd_press);
        if (rc) {
            return rc;
        }

        spd.spd_press_is_valid = dps368->validated;

        rc = data_func(sensor, data_arg, &spd, SENSOR_TYPE_PRESSURE);
    }

    if (type & SENSOR_TYPE_TEMPERATURE) {
        struct sensor_temp_data std;

        rc = dps368_get_temperature_degC(dps368, &std.std_temp);

        if (rc) {

            return rc;
        }

        std.std_temp_is_valid = dps368->validated;

        rc = data_func(sensor, data_arg, &std, SENSOR_TYPE_TEMPERATURE);
    }

    if (!(type & SENSOR_TYPE_TEMPERATURE) && !(type & SENSOR_TYPE_PRESSURE)) {
        return SYS_EINVAL;
    }

    return rc;
}

static int
dps368_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{

    /* If the read isn't looking for pressure, don't do anything. */
    if (!(type & (SENSOR_TYPE_PRESSURE | SENSOR_TYPE_TEMPERATURE))) {
        return SYS_EINVAL;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;

    return 0;
}

static int
dps368_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct dps368* dps368 = (struct dps368 *) SENSOR_GET_DEVICE(sensor);

    return dps368_config(dps368, (struct dps368_cfg_s*) cfg);
}


/*Bus- peripheral communication instantiations */
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
init_node_cb(struct bus_node *bnode, void *arg)
{
    struct sensor_itf *itf = arg;

    dps368_init((struct os_dev *)bnode, itf);
}

int
dps368_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                             const struct bus_i2c_node_cfg *i2c_cfg,
                             struct sensor_itf *sensor_itf)
{
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    sensor_itf->si_dev = &node->bnode.odev;
    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_i2c_node_create(name, node, i2c_cfg, sensor_itf);

    return rc;
}

int
dps368_create_spi_sensor_dev(struct bus_spi_node *node, const char *name,
                             const struct bus_spi_node_cfg *spi_cfg,
                             struct sensor_itf *sensor_itf)
{
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    sensor_itf->si_dev = &node->bnode.odev;
    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_spi_node_create(name, node, spi_cfg, sensor_itf);

    return rc;
}
#endif

