/**
 * Copyright (c) 2017-2025 Bosch Sensortec GmbH. All rights reserved.
 *
 * BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file       bmp5.c
 * @date       2025-08-13
 * @version    v1.0.0
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
#error "Bus driver not present, this driver is only supported with bus driver"
#endif

#include "sensor/sensor.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"
#include "bmp5/bmp5.h"
#include "bmp5_priv.h"
#include "hal/hal_gpio.h"
#include "modlog/modlog.h"
#include "stats/stats.h"
#include <syscfg/syscfg.h>
#include <console/console.h>

#define FIFOPARSE_DEBUG     0
#define CLEAR_INT_AFTER_ISR 0
#define BMP5_MAX_STREAM_MS  200000
#define BMP5_DEBUG          0
/*
 * Max time to wait for interrupt.
 */
#define BMP5_MAX_INT_WAIT (10 * OS_TICKS_PER_SEC)

/* Define stat names for querying */
STATS_NAME_START(bmp5_stat_section)
    STATS_NAME(bmp5_stat_section, write_errors)
    STATS_NAME(bmp5_stat_section, read_errors)
STATS_NAME_END(bmp5_stat_section)

/* Exports for the sensor API */
static int bmp5_sensor_read(struct sensor *sensor, sensor_type_t type,
                            sensor_data_func_t data_func, void *data_arg,
                            uint32_t timeout);

static int bmp5_sensor_get_config(struct sensor *sensor, sensor_type_t type,
                                  struct sensor_cfg *cfg);

static int bmp5_sensor_set_notification(struct sensor *sensor,
                                        sensor_event_type_t event);

static int bmp5_sensor_unset_notification(struct sensor *sensor,
                                          sensor_event_type_t event);

static int bmp5_sensor_handle_interrupt(struct sensor *sensor);

static int bmp5_sensor_set_config(struct sensor *sensor, void *cfg);

/** Internal API declarations */
static int set_pwr_ctrl_settings(struct bmp5_dev *dev);

static int set_odr_filter_settings(uint32_t desired_settings, struct bmp5_dev *dev);

static int bmp5_set_pwr_mode_bycfg(struct bmp5_dev *dev);

/* Sensor framework driver callbacks */
static const struct sensor_driver g_bmp5_sensor_driver = {
    .sd_read = bmp5_sensor_read,
    .sd_set_config = bmp5_sensor_set_config,
    .sd_get_config = bmp5_sensor_get_config,
    .sd_set_notification = bmp5_sensor_set_notification,
    .sd_unset_notification = bmp5_sensor_unset_notification,
    .sd_handle_interrupt = bmp5_sensor_handle_interrupt
};

static void
delay_msec(uint32_t delay)
{
    delay = (delay * OS_TICKS_PER_SEC) / 1000 + 1;
    os_time_delay(delay);
}

/**
 * Write multiple length data to BMP5 sensor over different interfaces
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp5_writelen(struct sensor_itf *itf, const uint8_t *payload, uint8_t len)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    return bus_node_simple_write(itf->si_dev, payload, len);
#else
    return SYS_ENOTSUP;
#endif
}

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
bmp5_readlen(struct bmp5 *bmp5, uint8_t reg, uint8_t *buffer, uint16_t len)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    uint8_t reg_buf[2] = { reg };
    uint16_t wlen = 1;
    return bus_node_simple_write_read_transact((struct os_dev *)bmp5, &reg_buf,
                                               wlen, buffer, len);
#else
    return SYS_ENOTSUP;
#endif
}

/**
 * @brief This API reads the data from the given register address of the sensor.
 */
int
bmp5_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint16_t len, struct bmp5_dev *dev)
{
    int rc;
    struct bmp5 *bmp5 = CONTAINER_OF(dev, struct bmp5, bmp5_dev);

    rc = bmp5_readlen(bmp5, reg_addr, reg_data, len);
    /* Check for communication error */
    if (rc) {
        STATS_INC(bmp5->stats, read_errors);
    }

    return rc;
}

/**
 * @brief This API writes the given data to the register address
 * of the sensor.
 */
int
bmp5_set_regs(const uint8_t *reg_addr, const uint8_t *reg_data, uint8_t len,
              struct bmp5_dev *dev)
{
    int rc;
    uint8_t len2 = (uint8_t)(len * 2);
    int i;
    uint8_t buf[len2];
    struct bmp5 *bmp5 = CONTAINER_OF(dev, struct bmp5, bmp5_dev);
    struct sensor_itf *itf = &bmp5->sensor.s_itf;

    /* Check for arguments validity */
    if (reg_addr && reg_data) {
        if (len) {
            /* Interleave address and data */
            for (i = 0; i < len2;) {
                buf[i++] = (uint8_t)(0x7F & *reg_addr++);
                buf[i++] = *reg_data++;
            }
            rc = bmp5_writelen(itf, buf, len2);
            /* Check for communication error */
            if (rc) {
                STATS_INC(bmp5->stats, write_errors);
            }
        } else {
            rc = SYS_EINVAL;
        }
    } else {
        rc = SYS_ENOENT;
    }

    return rc;
}

/**
 * @brief This internal API fills the register address and register data of
 * the over sampling settings for burst write operation.
 */
static void
fill_osr_data(uint32_t settings, uint8_t *addr, uint8_t *reg_data,
              struct bmp5_dev *dev, uint8_t *len)
{
    struct bmp5_odr_filter_settings osr_settings = dev->settings.odr_filter;

    if (settings & (BMP5_PRESS_OS_SEL | BMP5_TEMP_OS_SEL | BMP5_PRESS_EN_SEL)) {
        /* Temperature over sampling settings check */
        if (settings & BMP5_TEMP_OS_SEL) {
            /* Set the pressure over sampling settings in the
            register variable */
            reg_data[*len] = BMP5_SET_BITS_POS_0(reg_data[5], BMP5_TEMP_OS,
                                                 osr_settings.temp_os);
        }
        /* Pressure over sampling settings check */
        if (settings & BMP5_PRESS_OS_SEL) {
            /* Set the temperature over sampling settings in the
            register variable */
            reg_data[*len] |=
                BMP5_SET_BITS(reg_data[5], BMP5_PRESS_OS, osr_settings.press_os);
        }
        if (settings & BMP5_PRESS_EN_SEL) {
            /* Enable the pressure sensor */
            reg_data[*len] |= BMP5_SET_BITS(reg_data[5], BMP5_OSR_CNF_PRESS_EN,
                                            dev->settings.press_en);
        }
        addr[*len] = BMP5_OSR_CONFIG_ADDR;
        (*len)++;
    }
}

/**
 * @brief This internal API fills the register address and register data of
 * the odr settings for burst write operation.
 */
static void
fill_odr_data(uint8_t *addr, uint8_t *reg_data, struct bmp5_dev *dev, uint8_t *len)
{
    /* Limit the ODR to 0.125 Hz */
    if (dev->settings.odr_filter.odr > BMP5_ODR_0_125_HZ)
        dev->settings.odr_filter.odr = BMP5_ODR_0_125_HZ;
    /* We don't want to change the power mode, hence the OR
     * Set the odr settings in the register variable */
    reg_data[*len] =
        BMP5_SET_BITS(reg_data[6], BMP5_ODR, dev->settings.odr_filter.odr);
    addr[*len] = BMP5_ODR_CONFIG_ADDR;
    (*len)++;
}

/**
 * @brief This internal API fills the register address and register data of
 * the filter settings for burst write operation.
 */
static void
fill_filter_data(uint32_t settings, uint8_t *addr, uint8_t *reg_data,
                 struct bmp5_dev *dev, uint8_t *len)
{
    if (settings & BMP5_DSP_IIR_T_SEL) {
        /* Set the iir settings in the register variable */
        reg_data[*len] = BMP5_SET_BITS_POS_0(reg_data[0], BMP5_DSP_IIR_FILTER_T,
                                             dev->settings.odr_filter.iir_filter_t);
    }

    if (settings & BMP5_DSP_IIR_P_SEL) {
        /* Set the iir settings in the register variable */
        reg_data[*len] |= BMP5_SET_BITS(reg_data[0], BMP5_DSP_IIR_FILTER_P,
                                        dev->settings.odr_filter.iir_filter_p);
    }

    addr[*len] = BMP5_DSP_IIR_ADDR;
    (*len)++;
}

static int
set_power_mode(uint8_t pwr_mode, struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_data;
    uint8_t reg_addr = BMP5_ODR_CONFIG_ADDR;

    rc = bmp5_get_regs(reg_addr, &reg_data, 1, dev);
    if (!rc) {
        if (pwr_mode == BMP5_DEEP_STANDBY_MODE) {
            /* Set the deep standby mode */
            reg_data =
                BMP5_SET_BITS(reg_data, BMP5_ODR_CNF_DEEP_DIS, BMP5_DEEP_ENABLED);
        } else {
            /* Disable deep standby mode */
            reg_data =
                BMP5_SET_BITS(reg_data, BMP5_ODR_CNF_DEEP_DIS, BMP5_DEEP_DISABLED);
        }

        reg_data = BMP5_SET_BITS_POS_0(reg_data, BMP5_ODR_CNF_POWER_MODE, pwr_mode);

        rc = bmp5_set_regs(&reg_addr, &reg_data, 1, dev);
    }

    return rc;
}

/* Common error handler */
static inline int
bmp5_check_and_return(int rc, const char *func)
{
    if (rc) {
        BMP5_LOG_ERROR("%s: failed %d\n", func, rc);
    }
    return rc;
}

/**
 * @brief This API sets the pressure enable and temperature enable
 * settings of the sensor.
 */
static int
set_pwr_ctrl_settings(struct bmp5_dev *dev)
{
    int rc;

    /* Set the power mode settings in the register variable */
    rc = set_power_mode(dev->settings.pwr_mode, dev);

    return bmp5_check_and_return(rc, __func__);
}

/**
 * @brief This internal API gets the effective OSR configuration and ODR valid
 * status from the sensor.
 */
int
bmp5_get_osr_eff(struct bmp5_dev *dev, uint32_t *t_eff, uint32_t *p_eff)
{
    int rc = 0;
    uint8_t reg_data;
    uint8_t odr_is_valid = 0;

    /* Get effective OSR configuration and ODR valid status */
    rc = bmp5_get_regs(BMP5_OSR_EFF_ADDR, &reg_data, 1, dev);
    if (!rc) {
        odr_is_valid = BMP5_GET_BITS(reg_data, BMP5_OSR_EFF_ODR_IS_VALID);
        if (odr_is_valid) {
            *t_eff = BMP5_GET_BITS_POS_0(reg_data, BMP5_OSR_EFF_TEMP);
            *p_eff = BMP5_GET_BITS(reg_data, BMP5_OSR_EFF_PRESS);
        } else {
            /* If ODR is not valid, set effective OSR to 0 */
            *t_eff = 0;
            *p_eff = 0;
            rc = SYS_EINVAL;
        }
    }

    return rc;
}

/**
 * @brief This internal API sets the over sampling, odr and filter settings
 * of the sensor based on the settings selected by the user.
 */
static int
set_odr_filter_settings(uint32_t desired_settings, struct bmp5_dev *dev)
{
    int rc;
    /* No of registers to be configured is 3 */
    uint8_t reg_addr[3] = { 0 };
    /* No of register data to be read is 3 0x31, 0x36, 0x37 */
    uint8_t reg_data[7] = { 0 };
    uint8_t len = 0;

    rc = bmp5_get_regs(BMP5_DSP_IIR_ADDR, reg_data, 7, dev);
    if (!rc) {
        if (desired_settings & (BMP5_DSP_IIR_T_SEL | BMP5_DSP_IIR_P_SEL)) {
            /* Fill the iir filter register address and
            register data to be written in the sensor */
            fill_filter_data(desired_settings, reg_addr, reg_data, dev, &len);
        }
        if (desired_settings & (BMP5_PRESS_OS_SEL | BMP5_TEMP_OS_SEL)) {
            /* Fill the over sampling register address and
            register data to be written in the sensor */
            fill_osr_data(desired_settings, reg_addr, reg_data, dev, &len);
        }
        if (desired_settings & BMP5_ODR_SEL) {
            /* Fill the output data rate register address and
            register data to be written in the sensor */
            fill_odr_data(reg_addr, reg_data, dev, &len);
        }
        if (dev->settings.pwr_mode == BMP5_NORMAL_MODE) {
            /* TODO
             * In NORMAL mode:
             * After the sensor starts polling check the OSR_EFF.odr_is_valid
             * flag to see if the settings are valid. If they are not valid
             * the effective OSR will be:
             * For ODR >= 160 Hz, both OSRs will be set to 1.
             * For ODR < 160 Hz, both OSRs will be set to 2.
             * and the effective ODRs will be available in the
             * OSR_EFF.osr_t_eff and OSR_EFF.osr_p_eff.
             */
        }
        if (!rc && len) {
            /* Burst write the over sampling, odr and filter
            settings in the register */
            rc = bmp5_set_regs(reg_addr, reg_data, len, dev);
        }
    }

    return bmp5_check_and_return(rc, __func__);
}

/**
 * @brief This internal API sets the interrupt control (output mode, level,
 * mode and data ready) settings of the sensor based on the settings
 * selected by the user.
 */
static int
set_int_config_settings(uint32_t desired_settings, struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_data;
    uint8_t reg_addr;
    struct bmp5_int_config_settings int_settings;

    reg_addr = BMP5_INT_CONFIG_ADDR;
    rc = bmp5_get_regs(reg_addr, &reg_data, 1, dev);
    if (!rc) {
        int_settings = dev->settings.int_settings;

        if (desired_settings & BMP5_INT_OD) {
            /* Set the interrupt output mode bits */
            reg_data = BMP5_SET_BITS_POS_0(reg_data, BMP5_INT_OD, int_settings.od);
        }
        if (desired_settings & BMP5_INT_POL) {
            /* Set the interrupt level bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_INT_POL, int_settings.pol);
        }
        if (desired_settings & BMP5_INT_MODE) {
            /* Set the interrupt mode bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_INT_MODE, int_settings.mode);
        }
        if (desired_settings & BMP5_INT_PAD_DRV) {
            /* Set the interrupt mode bits */
            reg_data =
                BMP5_SET_BITS(reg_data, BMP5_INT_PAD_DRV, int_settings.pad_drv);
        }
        if (desired_settings & BMP5_INT_DRDY_EN) {
            /* Set the interrupt data ready bits */
            reg_data =
                BMP5_SET_BITS(reg_data, BMP5_INT_DRDY_EN, int_settings.drdy_en);
        }

        rc = bmp5_set_regs(&reg_addr, &reg_data, 1, dev);
    }

    return bmp5_check_and_return(rc, __func__);
}

/**
 * @brief This internal API sets the advance (i2c_wdt_en, i2c_wdt_sel)
 * settings of the sensor based on the settings selected by the user.
 */
static int
set_advance_settings(uint32_t desired_settings, struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_addr;
    uint8_t reg_data;
    struct bmp5_adv_settings adv_settings = dev->settings.adv_settings;

    reg_addr = BMP5_DRIVE_CONF_ADDR;
    rc = bmp5_get_regs(reg_addr, &reg_data, 1, dev);
    if (!rc) {
        if (desired_settings & BMP5_DRV_CNF_I2C_CSB_PULL_UP_EN) {
            /* Set the i2c csb pull up enable bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DRV_CNF_I2C_CSB_PUP_EN,
                                     adv_settings.i2c_csb_pull_up_en);
        }
#if MYNEWT_VAL(BMP5_SPI3_MODE_EN)
        if (desired_settings & BMP5_DRV_CNF_SPI3_MODE_EN) {
            /* Set the spi3 mode enable bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DRV_CNF_SPI3_EN,
                                     adv_settings.spi3_mode_en);
        }
#endif
        if (desired_settings & BMP5_DRV_CNF_PAD_IF_DRV) {
            /* Set the pad drive strength for serial IO pins SDZ/SDO */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DRV_CNF_PAD_IF_DRV_EN,
                                     adv_settings.pad_if_drv);
        }

        rc = bmp5_set_regs(&reg_addr, &reg_data, 1, dev);
    }

    reg_addr = BMP5_DSP_CONFIG_ADDR;
    rc = bmp5_get_regs(reg_addr, &reg_data, 1, dev);
    if (!rc) {
        if (desired_settings & BMP5_DSP_CNF_IIR_FLUSH_FORCED_EN) {
            /* Set the iir flush forced enable bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DSP_CNF_IIR_FLUSH_FORCED_EN,
                                     adv_settings.iir_flush_forced_en);
        }
        if (desired_settings & BMP5_DSP_CNF_IIR_SHADOW_SEL_T) {
            /* Set the shadow sel iir temperature bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DSP_CNF_IIR_SHADOW_SEL_T,
                                     adv_settings.iir_shadow_sel_t);
        }
        if (desired_settings & BMP5_DSP_CNF_IIR_FIFO_SEL_T) {
            /* Set the fifo selected iir temperature bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DSP_CNF_IIR_FIFO_SEL_T,
                                     adv_settings.fifo_sel_iir_t);
        }
        if (desired_settings & BMP5_DSP_CNF_IIR_SHADOW_SEL_P) {
            /* Set the shadow sel iir pressure bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DSP_CNF_IIR_SHADOW_SEL_P,
                                     adv_settings.iir_shadow_sel_p);
        }
        if (desired_settings & BMP5_DSP_CNF_IIR_FIFO_SEL_P) {
            /* Set the fifo selected iir pressure bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DSP_CNF_IIR_FIFO_SEL_P,
                                     adv_settings.fifo_sel_iir_p);
        }
        if (desired_settings & BMP5_DSP_CNF_OOR_SEL_IIR_P) {
            /* Set the oor sel iir pressure bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DSP_CNF_OOR_SEL_IIR_P,
                                     adv_settings.oor_sel_iir_p);
        }

        rc = bmp5_set_regs(&reg_addr, &reg_data, 1, dev);
    }

    return bmp5_check_and_return(rc, __func__);
}

/**
 * @brief This API sets compensation settings of the sensor.
 */
static int
set_temp_press_compensate(uint32_t desired_settings, struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_addr;
    uint8_t reg_data;

    reg_addr = BMP5_DSP_CONFIG_ADDR;
    rc = bmp5_get_regs(reg_addr, &reg_data, 1, dev);
    if (!rc) {
        if (desired_settings & BMP5_TEMP_COMP_EN) {
            /* Set the temperature compensation enable bits */
            reg_data = BMP5_SET_BITS_POS_0(reg_data, BMP5_DSP_CNF_T_COMP_EN,
                                           dev->settings.temp_comp_en);
        }
        if (desired_settings & BMP5_PRESS_COMP_EN) {
            /* Set the pressure compensation enable bits */
            reg_data = BMP5_SET_BITS(reg_data, BMP5_DSP_CNF_P_COMP_EN,
                                     dev->settings.press_comp_en);
        }
        rc = bmp5_set_regs(&reg_addr, &reg_data, 1, dev);
    }
    return rc;
}

/**
 * @brief This API gets the power mode of the sensor.
 */
int
bmp5_get_pwr_mode(uint8_t *pwr_mode, struct bmp5_dev *dev)
{
    int rc;

    /* Read the power mode register */
    rc = bmp5_get_regs(BMP5_ODR_CONFIG_ADDR, pwr_mode, 1, dev);
    if (!rc) {
        *pwr_mode = BMP5_GET_BITS_POS_0(*pwr_mode, BMP5_ODR_CNF_POWER_MODE);
    }

    return rc;
}

/**
 * @brief This API sets the power control(pressure enable and
 * temperature enable), over sampling, odr and filter
 * settings in the sensor.
 */
int
bmp5_set_sensor_settings(uint32_t desired_settings, struct bmp5_dev *dev)
{
    int rc = 0;

    if ((desired_settings & BMP5_POWER_MODE_SEL) == 0) {
        /* Put the device to standby mode before changing any settings */
        rc = set_power_mode(BMP5_STANDBY_MODE, dev);
        if (rc) {
            goto err;
        }
    }
    /* Give some time for device to go into sleep mode */
    delay_msec(2);

    if ((desired_settings & BMP5_ODR_FILTER) && (!rc)) {
        /* Set the over sampling, odr and filter settings */
        rc = set_odr_filter_settings(desired_settings, dev);
    }
    if ((desired_settings & BMP5_INT_CONFIG) && (!rc)) {
        /* Set the interrupt control settings */
        rc = set_int_config_settings(desired_settings, dev);
    }
    if ((desired_settings & BMP5_DRIVE_CONFIG) && (!rc)) {
        /* Set the advance settings */
        rc = set_advance_settings(desired_settings, dev);
    }
    if ((desired_settings & BMP5_COMPENSATE) && (!rc)) {
        /* Set the power mode */
        rc = set_temp_press_compensate(desired_settings, dev);
    }

    if ((desired_settings & BMP5_POWER_MODE_SEL) == 0) {
        /* Force set the power control settings without
         * reading the existing power mode
         */
        rc = set_pwr_ctrl_settings(dev);
    } else if (!rc) {
        /* Read the most recent power mode, if the device is in the same power
         * as what we want to configure, don't reconfigure, if not, put the
         * sensor into the requested power mode.
         */
        rc = bmp5_set_pwr_mode_bycfg(dev);
    }
err:
    return bmp5_check_and_return(rc, __func__);
}

/**
 * @brief This API sets the power mode of the sensor.
 */
static int
bmp5_set_pwr_mode_bycfg(struct bmp5_dev *dev)
{
    int rc;
    uint8_t last_set_mode;
    uint8_t curr_mode = dev->settings.pwr_mode;

    rc = bmp5_get_pwr_mode(&last_set_mode, dev);

    /* quit if sensor is already in the requested mode */
    if (last_set_mode == curr_mode) {
        return 0;
    }

    /* If the sensor is not in sleep mode put the device to sleep mode */
    if (last_set_mode != BMP5_STANDBY_MODE) {
        /* Device should be put to sleep before transiting to
        forced mode or normal mode */
        rc = set_power_mode(BMP5_STANDBY_MODE, dev);
        /* Give some time for device to go into sleep mode */
        delay_msec(5);
    }
    /* Set the power mode */
    if (!rc) {
        if (curr_mode == BMP5_NORMAL_MODE || curr_mode == BMP5_FORCED_MODE ||
            curr_mode == BMP5_DEEP_STANDBY_MODE || curr_mode == BMP5_CONTINUOUS_MODE) {
            rc = set_power_mode(curr_mode, dev);
            delay_msec(5);
        } else if (curr_mode == BMP5_STANDBY_MODE) {
            /* do nothing, we are already in standby mode */
        } else {
            /* Invalid power mode */
            rc = SYS_EINVAL;
        }
    }
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_normal_mode(struct bmp5 *bmp5)
{
    int rc;
    /* Used to select the settings user needs to change */
    uint32_t settings_sel;

    /* Select the pressure and temperature sensor to be enabled */
    bmp5->bmp5_dev.settings.press_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.temp_en = BMP5_ENABLE;
    /* Enable temperature and pressure compensation */
    bmp5->bmp5_dev.settings.temp_comp_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.press_comp_en = BMP5_ENABLE;
    /* Select the output data rate and oversampling settings for pressure and temperature */
    bmp5->bmp5_dev.settings.odr_filter.press_os = BMP5_NO_OVERSAMPLING;
    bmp5->bmp5_dev.settings.odr_filter.temp_os = BMP5_NO_OVERSAMPLING;
    /* Set the power mode to normal mode */
    bmp5->bmp5_dev.settings.pwr_mode = BMP5_NORMAL_MODE;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP5_POWER_MODE_SEL | BMP5_PRESS_EN_SEL | BMP5_TEMP_EN_SEL |
                   BMP5_PRESS_OS_SEL | BMP5_TEMP_OS_SEL | BMP5_ODR_SEL |
                   BMP5_COMPENSATE;
    rc = bmp5_set_sensor_settings(settings_sel, &bmp5->bmp5_dev);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_forced_mode_with_osr(struct bmp5 *bmp5)
{
    int rc;
    /* Used to select the settings user needs to change */
    uint32_t settings_sel;

    /* Select the pressure and temperature sensor to be enabled */
    bmp5->bmp5_dev.settings.press_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.temp_en = BMP5_ENABLE;
    /* Enable temperature and pressure compensation */
    bmp5->bmp5_dev.settings.temp_comp_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.press_comp_en = BMP5_ENABLE;
    /* Select the power mode */
    bmp5->bmp5_dev.settings.pwr_mode = BMP5_FORCED_MODE;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP5_POWER_MODE_SEL | BMP5_PRESS_EN_SEL | BMP5_TEMP_EN_SEL |
                   BMP5_PRESS_OS_SEL | BMP5_TEMP_OS_SEL | BMP5_ODR_SEL |
                   BMP5_COMPENSATE;
    /* Write the settings in the sensor */
    rc = bmp5_set_sensor_settings(settings_sel, &bmp5->bmp5_dev);
    return bmp5_check_and_return(rc, __func__);
}

/**
 *  @brief This internal API is used to parse the pressure or temperature or
 *  both the data and store it in the bmp5_data structure instance.
 */
static void
parse_sensor_data(const uint8_t *reg_data, struct bmp5_data *data)
{
    /* Store the parsed register values for temperature data */
    data->temperature =
        (((int32_t)((int32_t)((uint32_t)(((uint32_t)reg_data[2] << 16) |
                                         ((uint16_t)reg_data[1] << 8) | reg_data[0])
                              << 8) >>
                    8)) /
         65536.0f) *
        100.0f;

    /* Store the parsed register values for pressure data */
    data->pressure = (((uint32_t)((uint32_t)(reg_data[5] << 16) |
                                  (uint16_t)(reg_data[4] << 8) | reg_data[3])) /
                      64.0f) *
                     100.0f;
}

/**
 * @brief This API reads the pressure, temperature or both data from the
 * sensor and stores it in the bmp5_data structure instance passed by the
 * user.
 */
static int
bmp5_read_sensor_data(uint8_t sensor_comp, struct bmp5 *dev, struct bmp5_data *comp_data)
{
    int rc;
    /* Array to store the pressure and temperature data read from the sensor */
    uint8_t reg_data[BMP5_P_T_DATA_LEN] = { 0 };

    if (comp_data) {
        /* Read the pressure and temperature data from the sensor */
        rc = bmp5_get_regs(BMP5_DATA_ADDR, reg_data, BMP5_P_T_DATA_LEN, &dev->bmp5_dev);
        if (!rc) {
            /* Parse the read data from the sensor */
            parse_sensor_data(reg_data, comp_data);
        }
    } else {
        rc = SYS_ENOENT;
    }

    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_get_sensor_data(struct bmp5 *bmp5, struct bmp5_data *sensor_data)
{
    /* Variable used to select the sensor component */
    uint8_t sensor_comp;

    /* Sensor component selection */
    sensor_comp = BMP5_PRESS | BMP5_TEMP;
    /* Temperature and Pressure data are read and stored in the bmp5_data instance */
    return bmp5_read_sensor_data(sensor_comp, bmp5, sensor_data);
}

/**
 * @brief This API performs FIFO flush of the sensor.
 */
int
bmp5_fifo_flush(struct bmp5_dev *dev)
{
    struct bmp5 *bmp5 = CONTAINER_OF(dev, struct bmp5, bmp5_dev);

    /* An ODR/OSR reconfig causes a FIFO flush, just reconfigure OSR */
    return bmp5_set_rate(bmp5, bmp5->cfg.rate);
}

/**
 * @brief This API performs the soft reset of the sensor.
 */
int
bmp5_soft_reset(struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_addr = BMP5_CMD_ADDR;
    /* 0xB6 is the soft reset command */
    uint8_t flush_rst_cmd = 0xB6;
    uint8_t status = 0;

    /* Check for command ready status */
    rc = bmp5_get_regs(BMP5_STATUS_REG_ADDR, &status, 1, dev);
    /* Device is ready to accept new command */
    if ((status & BMP5_NVM_RDY) && (!rc)) {
        /* Write the soft reset command in the sensor */
        rc = bmp5_set_regs(&reg_addr, &flush_rst_cmd, 1, dev);
        /* Proceed if everything is fine until now */
        if (!rc) {
            /* Wait for 2 ms */
            delay_msec(2);
            /* Read for error */
            rc = bmp5_get_regs(BMP5_STATUS_REG_ADDR, &status, 1, dev);
            /* check for error status */
            if ((!(status & BMP5_CORE_RDY) || !(status & BMP5_NVM_RDY) ||
                 (status & BMP5_NVM_ERR)) &&
                (rc)) {
                /* Sensor did not come up fine, so return error */
                if (!rc) {
                    rc = SYS_EINVAL;
                }
            }
        }
    } else {
        if (!rc) {
            /* Command not ready hence return error */
            rc = SYS_EINVAL;
        }
    }

    return bmp5_check_and_return(rc, __func__);
}

/**
 *  @brief This API is the entry point.
 *  It performs the selection of I2C interface and
 *  reads the chip-id of the sensor.
 */
int
bmp5_itf_init(struct bmp5 *dev)
{
    int rc;
    uint8_t chip_id = 0;

    /* Read the chip-id of bmp5 sensor */
    rc = bmp5_get_chip_id(dev, &chip_id);
    /* Proceed if everything is fine until now */
    if (!rc) {
        /* Check for chip id validity */
        if ((chip_id == BMP581_BMP580_CHIP_ID) || (chip_id == BMP585_CHIP_ID)) {
            dev->bmp5_dev.chip_id = chip_id;
            /* Reset the sensor */
            rc = bmp5_soft_reset(&dev->bmp5_dev);
        } else {
            rc = SYS_EINVAL;
        }
#if BMP5_DEBUG
        BMP5_LOG_ERROR("%s:chip ID  0x%x\n", __func__, chip_id);
#endif
    }
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_get_chip_id(struct bmp5 *bmp5, uint8_t *chip_id)
{
    uint8_t reg;
    int rc;
    rc = bmp5_get_regs(BMP5_CHIP_ID_ADDR, &reg, 1, &bmp5->bmp5_dev);
    if (rc) {
        goto err;
    }

    *chip_id = reg;

err:
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_dump(struct bmp5 *bmp5)
{
    uint8_t val;
    uint8_t index = 0;
    int rc;

    /* Dump all the register values for debug purposes */
    for (index = 0; index < 0x7F; index++) {
        val = 0;
        rc = bmp5_get_regs(index, &val, 1, &bmp5->bmp5_dev);
        if (rc) {
            goto err;
        }
        console_printf("register 0x%02X   0x%02X\n", index, val);
    }

err:
    return rc;
}

/**
 * @brief This internal API fills the fifo interrupt control(fths_en, ffull_en)
 * settings in the reg_data variable so as to burst write in the sensor.
 */
static void
fill_fifo_int_config(uint16_t desired_settings, uint8_t *reg_data,
                     const struct bmp5_fifo_settings *dev_fifo)
{
    if (desired_settings & BMP5_FIFO_FTHS_EN_SEL) {
        /* Fill the FIFO threshold interrupt enable data */
        *reg_data = BMP5_SET_BITS(*reg_data, BMP5_INT_STATUS_FTHS, dev_fifo->fths_en);
    }
    if (desired_settings & BMP5_FIFO_FFULL_EN_SEL) {
        /* Fill the FIFO full interrupt enable data */
        *reg_data =
            BMP5_SET_BITS(*reg_data, BMP5_INT_STATUS_FFULL, dev_fifo->ffull_en);
    }
}

/**
 * @brief This API sets FIFO selection configurations in the sensor.
 *
 * @param desired_settings The settings to be set in the sensor.
 * @param dev Pointer to the bmp5_dev structure.
 */
int
bmp5_set_fifo_selection_config(uint16_t desired_settings, struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_addr = BMP5_FIFO_SEL_ADDR;
    uint8_t reg_data;

    if (dev->fifo) {
        rc = bmp5_get_regs(reg_addr, &reg_data, 1, dev);
        if (!rc) {
            if (desired_settings & BMP5_FIFO_TEMP_EN_SEL) {
                reg_data = BMP5_SET_BITS_POS_0(reg_data, BMP5_FIFO_FRAME_SEL_TEMP_EN,
                                               dev->fifo->settings.temp_en);
            }
            if (desired_settings & BMP5_FIFO_PRESS_EN_SEL) {
                reg_data = BMP5_SET_BITS(reg_data, BMP5_FIFO_FRAME_SEL_PRESS_EN,
                                         dev->fifo->settings.press_en);
            }
            if (desired_settings & BMP5_FIFO_DECIMENT_SEL) {
                /* Fill the FIFO decimation selection (downsampling) */
                reg_data = BMP5_SET_BITS(reg_data, BMP5_FIFO_DECIMENT_SEL,
                                         dev->fifo->settings.dec_sel);
            }
            /* Write the FIFO selection settings in the sensor */
            rc = bmp5_set_regs(&reg_addr, &reg_data, 1, dev);
        }
    } else {
        rc = SYS_ENOENT;
    }

    return bmp5_check_and_return(rc, __func__);
}

/**
 * @brief This API sets the FIFO selection configsurations in the sensor.
 * and also sets the int_config(fths_en, ffull_en) settings in the sensor.
 */
int
bmp5_set_fifo_settings(uint16_t desired_settings, struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_addr[2] = { BMP5_FIFO_CONFIG_ADDR, BMP5_INT_CONFIG_ADDR };
    uint8_t reg_data[2] = { 0 };
    uint8_t len = 2;

    if (dev->fifo) {
        rc = bmp5_get_regs(reg_addr[0], reg_data, len, dev);
        if (!rc) {
            /* Fill the FIFO mode settings */
            if (desired_settings & BMP5_FIFO_MODE_SEL) {
                reg_data[0] = BMP5_SET_BITS(reg_data[0], BMP5_FIFO_MODE,
                                            dev->fifo->settings.mode);
            }
            if (desired_settings & FIFO_INT_CONFIG) {
                /* Fill the FIFO interrupt config register */
                fill_fifo_int_config(desired_settings, &reg_data[1],
                                     &dev->fifo->settings);
            }
            /* Write the FIFO settings in the sensor */
            rc = bmp5_set_regs(reg_addr, reg_data, 2, dev);

            if (desired_settings & BMP5_FIFO_SEL_CONFIG) {
                /* Fill the FIFO selection config register */
                rc |= bmp5_set_fifo_selection_config(desired_settings, dev);
            }
        }
    } else {
        rc = SYS_ENOENT;
    }

    return bmp5_check_and_return(rc, __func__);
}

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
/**
 * @brief This internal API converts the no. of frames required by the user to
 * bytes so as to write in the threshold length register.
 */
static int
convert_frames_to_bytes(uint16_t *threshold_len, struct bmp5_dev *dev)
{
    int rc = 0;

    if ((dev->fifo->data.req_frames > 0) &&
        (dev->fifo->data.req_frames <= BMP5_FIFO_MAX_FRAMES)) {
        if (dev->fifo->settings.press_en && dev->fifo->settings.temp_en) {
            /* Multiply with pressure and temperature len */
            *threshold_len = dev->fifo->data.req_frames * BMP5_P_AND_T_DATA_LEN;
        } else if (dev->fifo->settings.temp_en || dev->fifo->settings.press_en) {
            /* Multiply with pressure or temperature len */
            *threshold_len = dev->fifo->data.req_frames * BMP5_P_OR_T_DATA_LEN;
        } else {
            /* No sensor is enabled */
            rc = SYS_ENOENT;
        }
    } else {
        /* Required frame count is zero, which is invalid */
        rc = SYS_EINVAL;
    }

    return rc;
}

/**
 * @brief This API sets the fifo threshold length according to the frames count
 * set by the user in the device structure. Refer below for usage.
 */
int
bmp5_set_fifo_threshold(struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_data;
    uint8_t reg_addr;
    uint16_t threshold_len;

    reg_addr = BMP5_FIFO_CONFIG_ADDR;
    if (dev->fifo) {
        /* Get the fifo threshold length in bytes */
        rc = bmp5_get_regs(reg_addr, &reg_data, 1, dev);
        if (rc) {
            goto err;
        }
        rc = convert_frames_to_bytes(&threshold_len, dev);
        if (!rc) {
            reg_data &= threshold_len & 0x1F; /* Get the lower 5 bits */
            rc = bmp5_set_regs(&reg_addr, &reg_data, 1, dev);
        }
    } else {
        rc = SYS_ENOENT;
    }

err:
    return bmp5_check_and_return(rc, __func__);
}

// Combine similar FIFO configuration functions
static int
bmp5_configure_fifo_common(struct bmp5 *bmp5, uint8_t int_type, uint8_t enable)
{
    struct bmp5_fifo fifo = {
        .settings = {
            .mode = BMP5_ENABLE,
            .press_en = BMP5_ENABLE,
            .temp_en = BMP5_ENABLE,
            .dec_sel = BMP5_FIFO_NO_DOWNSAMPLING,
        }
    };
    uint32_t settings_sel;

    if (int_type == BMP5_FIFO_THS_INT) {
        fifo.settings.fths_en = enable;
        fifo.data.req_frames = bmp5->bmp5_dev.fifo_threshold_level;
        settings_sel = BMP5_FIFO_MODE_SEL | BMP5_FIFO_PRESS_EN_SEL |
                       BMP5_FIFO_TEMP_EN_SEL | BMP5_FIFO_DECIMENT_SEL |
                       BMP5_FIFO_FTHS_EN_SEL;
    } else {
        fifo.settings.ffull_en = enable;
        settings_sel = BMP5_FIFO_MODE_SEL | BMP5_FIFO_PRESS_EN_SEL |
                       BMP5_FIFO_TEMP_EN_SEL | BMP5_FIFO_DECIMENT_SEL |
                       BMP5_FIFO_FFULL_EN_SEL;
    }

    bmp5->bmp5_dev.fifo = &fifo;
    return bmp5_set_fifo_settings(settings_sel, &bmp5->bmp5_dev);
}

int
bmp5_configure_fifo_with_threshold(struct bmp5 *bmp5, uint8_t en)
{
    int rc;
    rc = bmp5_configure_fifo_common(bmp5, BMP5_FIFO_THS_INT, en);
    if (rc) {
        goto err;
    }
    rc = bmp5_set_fifo_threshold(&bmp5->bmp5_dev);
err:
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_configure_fifo_with_fifofull(struct bmp5 *bmp5, uint8_t en)
{
    int rc;
    rc = bmp5_configure_fifo_common(bmp5, BMP5_FIFO_FULL_INT, en);
    return bmp5_check_and_return(rc, __func__);
}
#endif

int
bmp5_enable_fifo(struct bmp5 *bmp5, uint8_t en)
{
    int rc;
    /* FIFO object to be assigned to device structure */
    struct bmp5_fifo fifo;
    /* Used to select the settings user needs to change */
    uint32_t settings_sel;

    /* Enable fifo */
    fifo.settings.mode = en;
    /* Enable Pressure sensor for fifo */
    fifo.settings.press_en = BMP5_ENABLE;
    /* Enable temperature sensor for fifo */
    fifo.settings.temp_en = BMP5_ENABLE;
    /* No subsampling for FIFO */
    fifo.settings.dec_sel = BMP5_FIFO_NO_DOWNSAMPLING;
    /* Link the fifo object to device structure */
    bmp5->bmp5_dev.fifo = &fifo;
    /* Select the settings required for fifo */
    settings_sel = BMP5_FIFO_MODE_SEL | BMP5_FIFO_PRESS_EN_SEL |
                   BMP5_FIFO_TEMP_EN_SEL | BMP5_FIFO_DECIMENT_SEL;
    /* Set the selected settings in fifo */
    rc = bmp5_set_fifo_settings(settings_sel, &bmp5->bmp5_dev);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_rate(struct bmp5 *bmp5, uint8_t rate)
{
    int rc;
    /* Used to select the settings user needs to change */
    uint32_t settings_sel;

    /* Select the pressure and temperature sensor to be enabled */
    bmp5->bmp5_dev.settings.press_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.temp_en = BMP5_ENABLE;
    /* Select the output data rate settings for pressure and temperature */
    bmp5->bmp5_dev.settings.odr_filter.odr = rate;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP5_PRESS_EN_SEL | BMP5_ODR_SEL;
    rc = bmp5_set_sensor_settings(settings_sel, &bmp5->bmp5_dev);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_power_mode(struct bmp5 *bmp5, uint8_t mode)
{
    int rc = 0;
    /* Select the power mode */
    bmp5->bmp5_dev.settings.pwr_mode = mode;
    /* Set the power mode in the sensor */
    rc = bmp5_set_pwr_mode_bycfg(&bmp5->bmp5_dev);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_press_temp_compensate(struct bmp5 *bmp5, uint8_t temp_comp_en,
                               uint8_t press_comp_en)
{
    int rc;
    /* Used to select the settings user needs to change */
    uint32_t settings_sel;

    /* Select the pressure and temperature sensor to be enabled */
    bmp5->bmp5_dev.settings.press_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.temp_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.press_comp_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.temp_comp_en = BMP5_ENABLE;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP5_PRESS_EN_SEL | BMP5_TEMP_EN_SEL | BMP5_TEMP_COMP_EN |
                   BMP5_PRESS_COMP_EN;
    rc = bmp5_set_sensor_settings(settings_sel, &bmp5->bmp5_dev);
    return bmp5_check_and_return(rc, __func__);
}

static int
bmp5_set_int_setting(struct bmp5 *bmp5, uint32_t setting_mask, uint8_t value)
{
    uint32_t settings_sel;
    int rc;

    /* Common settings */
    bmp5->bmp5_dev.settings.press_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.temp_en = BMP5_ENABLE;

    /* Apply specific setting based on mask */
    switch (setting_mask) {
    case BMP5_INT_OD:
        bmp5->bmp5_dev.settings.int_settings.od = value;
        break;
    case BMP5_INT_MODE:
        bmp5->bmp5_dev.settings.int_settings.mode = value;
        break;
    case BMP5_INT_POL:
        bmp5->bmp5_dev.settings.int_settings.pol = value;
        break;
    case BMP5_INT_PAD_DRV:
        bmp5->bmp5_dev.settings.int_settings.pad_drv = value;
        break;
    case BMP5_INT_DRDY_EN:
        bmp5->bmp5_dev.settings.int_settings.drdy_en = value;
        break;
    default:
        rc = SYS_EINVAL;
        goto err;
    }

    settings_sel = BMP5_PRESS_EN_SEL | BMP5_TEMP_EN_SEL | setting_mask;
    rc = bmp5_set_sensor_settings(settings_sel, &bmp5->bmp5_dev);
err:
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_int_pp_od(struct bmp5 *bmp5, uint8_t mode)
{
    int rc;
    rc = bmp5_set_int_setting(bmp5, BMP5_INT_OD, mode);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_int_pad_drv(struct bmp5 *bmp5, uint8_t drv)
{
    int rc;
    rc = bmp5_set_int_setting(bmp5, BMP5_INT_PAD_DRV, drv);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_int_mode(struct bmp5 *bmp5, uint8_t en)
{
    int rc;
    rc = bmp5_set_int_setting(bmp5, BMP5_INT_MODE, en);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_int_active_pol(struct bmp5 *bmp5, uint8_t pol)
{
    int rc;
    rc = bmp5_set_int_setting(bmp5, BMP5_INT_POL, pol);
    return bmp5_check_and_return(rc, __func__);
}

/**
 * Sets whether data ready interrupt are enabled
 *
 * @param The sensor interface
 * @param value to set (0 = not mode, 1 = mode)
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp5_set_drdy_int(struct bmp5 *bmp5, uint8_t en)
{
    int rc;
    rc = bmp5_set_int_setting(bmp5, BMP5_INT_DRDY_EN, en);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_filter_cfg(struct bmp5 *bmp5, uint8_t press_osr, uint8_t temp_osr)
{
    /* Used to select the settings user needs to change */
    uint32_t settings_sel;
    int rc = 0;

    /* Select the pressure and temperature sensor to be enabled */
    bmp5->bmp5_dev.settings.press_en = BMP5_ENABLE;
    bmp5->bmp5_dev.settings.temp_en = BMP5_ENABLE;
    /* Select the oversampling settings for pressure and temperature */
    bmp5->bmp5_dev.settings.odr_filter.press_os = press_osr;
    bmp5->bmp5_dev.settings.odr_filter.temp_os = temp_osr;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP5_PRESS_EN_SEL | BMP5_TEMP_EN_SEL | BMP5_PRESS_OS_SEL |
                   BMP5_TEMP_OS_SEL;
    /* Write the settings in the sensor */
    rc = bmp5_set_sensor_settings(settings_sel, &bmp5->bmp5_dev);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_fifo_cfg(struct bmp5 *bmp5, enum bmp5_fifo_mode mode, uint8_t fifo_ths)
{
    int rc = 0;

    bmp5->bmp5_dev.fifo_threshold_level = fifo_ths;
#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    if (mode == BMP5_FIFO_M_FIFO) {
        rc = bmp5_enable_fifo(bmp5, BMP5_ENABLE);
    } else {
        rc = bmp5_enable_fifo(bmp5, BMP5_DISABLE);
    }
#else
    rc = bmp5_enable_fifo(bmp5, BMP5_DISABLE);

#endif
    return bmp5_check_and_return(rc, __func__);
}

#if MYNEWT_VAL(BMP5_INT_ENABLE)
int
bmp5_clear_int(struct bmp5 *bmp5)
{
    int rc = 0;
    uint8_t reg_addr;
    uint8_t reg_data;

    reg_addr = BMP5_INT_CONFIG_ADDR;
    rc = bmp5_get_regs(reg_addr, &reg_data, 1, &bmp5->bmp5_dev);
    if (!rc) {
        bmp5->bmp5_dev.settings.int_settings.drdy_en = BMP5_DISABLE;
        reg_data = BMP5_SET_BITS_POS_0(reg_data, BMP5_INT_STATUS_DRDY, BMP5_DISABLE);
        reg_data = BMP5_SET_BITS(reg_data, BMP5_INT_STATUS_FTHS, BMP5_DISABLE);
        reg_data = BMP5_SET_BITS(reg_data, BMP5_INT_STATUS_FFULL, BMP5_DISABLE);
        rc = bmp5_set_regs(&reg_addr, &reg_data, 1, &bmp5->bmp5_dev);
    }
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_set_int_enable(struct bmp5 *bmp5, uint8_t enabled, enum bmp5_int_type int_type)
{
    int rc = 0;

#if BMP5_DEBUG
    BMP5_LOG_ERROR("%s:start to set %d int\n", __func__, int_type);
#endif
    switch (int_type) {
    case BMP5_DRDY_INT:
        rc = bmp5_set_drdy_int(bmp5, enabled);
        if (rc) {
            goto err;
        }

        rc = bmp5_set_normal_mode(bmp5);
        if (rc) {
            goto err;
        }
        break;
#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    case BMP5_FIFO_THS_INT:
        rc = bmp5_configure_fifo_with_threshold(bmp5, enabled);
        if (rc) {
            goto err;
        }

        rc = bmp5_set_normal_mode(bmp5);
        if (rc) {
            goto err;
        }
        break;
    case BMP5_FIFO_FULL_INT:
        rc = bmp5_configure_fifo_with_fifofull(bmp5, enabled);
        if (rc) {
            goto err;
        }
        rc = bmp5_set_normal_mode(bmp5);
        if (rc) {
            goto err;
        }
        break;
#endif
    default:
        rc = SYS_EINVAL;
        goto err;
    }
    return 0;
err:
    return bmp5_check_and_return(rc, __func__);
}
#endif

/**
 * @brief This API gets the NVM ready, NVM error and
 * power on reset status from the sensor.
 *
 * @param dev   Structure instance of bmp5_dev
 *
 * @return Result of API execution status.
 * @retval zero -> Success / -ve value -> Error.
 */
static int
get_sensor_status(struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_addr;
    uint8_t reg_data;

    reg_addr = BMP5_STATUS_REG_ADDR;
    rc = bmp5_get_regs(reg_addr, &reg_data, 1, dev);

    if (!rc) {
        dev->status.sensor.nvm_rdy = BMP5_GET_BITS(reg_data, BMP5_STATUS_NVM_RDY);
        dev->status.sensor.nvm_err = BMP5_GET_BITS(reg_data, BMP5_STATUS_NVM_ERR);

        reg_addr = BMP5_INT_STATUS_REG_ADDR;
        rc = bmp5_get_regs(reg_addr, &reg_data, 1, dev);
        if (rc) {
            goto err;
        }
        /* Proceed if read from register is fine */
        dev->status.pwr_on_rst = reg_data & 0x10 ? 1 : 0;
    }

err:
    return bmp5_check_and_return(rc, __func__);
}

#if MYNEWT_VAL(BMP5_INT_ENABLE)
/**
 * @brief This API gets the interrupt (fifo threshold, fifo full, data ready)
 * status from the sensor.
 *
 * @param dev   Structure instance of bmp5_dev
 *
 * @return Result of API execution status.
 * @retval zero -> Success / -ve value -> Error.
 */
static int
get_int_status(struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_data;

    rc = bmp5_get_regs(BMP5_INT_STATUS_REG_ADDR, &reg_data, 1, dev);
    if (!rc) {
        dev->status.intr.drdy = BMP5_GET_BITS_POS_0(reg_data, BMP5_INT_STATUS_DRDY);
        dev->status.intr.fifo_full = BMP5_GET_BITS(reg_data, BMP5_INT_STATUS_FFULL);
        dev->status.intr.fifo_ths = BMP5_GET_BITS(reg_data, BMP5_INT_STATUS_FTHS);
        dev->status.intr.oor_p = BMP5_GET_BITS(reg_data, BMP5_INT_STATUS_OOR_P);
    }
    return bmp5_check_and_return(rc, __func__);
}
#endif

/**
 * @brief This API gets the sensor status
 */
int
bmp5_get_status(struct bmp5_dev *dev)
{
    int rc;

    rc = get_sensor_status(dev);

#if MYNEWT_VAL(BMP5_INT_ENABLE)
    /* Proceed further if the earlier operation is fine */
    if (!rc) {
        rc = get_int_status(dev);
    }
#endif

    return rc;
}

/**
 * @brief This internal API resets the FIFO buffer, start index,
 * parsed frame count, configuration change, configuration error and
 * frame_not_available variables.
 *
 * @param fifo   FIFO structure instance where the fifo related variables
 * are reset.
 */
static void
reset_fifo_index(struct bmp5_fifo *fifo)
{
    memset(fifo->data.buffer, 0, sizeof(fifo->data.buffer));

    fifo->data.byte_count = 0;
    fifo->data.start_idx = 0;
    fifo->data.parsed_frames = 0;
    fifo->data.config_change = 0;
    fifo->data.config_err = 0;
    fifo->data.frame_not_available = false;
}

/**
 * @brief This API gets the fifo length from the sensor.
 */
int
bmp5_get_fifo_count(uint16_t *fifo_count, struct bmp5_dev *dev)
{
    int rc;
    uint8_t reg_data[2];

    rc = bmp5_get_regs(BMP5_FIFO_COUNT_ADDR, reg_data, 2, dev);
    /* Proceed if read from register is fine */
    if (!rc) {
        /* Update the fifo length */
        *fifo_count = BMP5_CONCAT_BYTES(reg_data[1], reg_data[0]);
    }
    return bmp5_check_and_return(rc, __func__);
}

/**
 * @brief This API gets the fifo data from the sensor.
 */
int
bmp5_get_fifo_data(struct bmp5_dev *dev)
{
    int rc;
    uint16_t fifo_len;
    struct bmp5_fifo *fifo = dev->fifo;
#if FIFOPARSE_DEBUG
    uint16_t i;
#endif

    if (fifo) {
        reset_fifo_index(fifo);
        /* Get the total no of bytes available in FIFO */
        rc = bmp5_get_fifo_count(&fifo_len, dev);
#if BMP5_DEBUG
        BMP5_LOG_ERROR("fifo_len is %d\n", fifo_len);
#endif
        if (rc) {
            goto err;
        }

        if (fifo_len > sizeof(fifo->data.buffer)) {
            rc = SYS_ENOMEM;
        }

        if (!rc) {
            /* Update the fifo length in the fifo structure */
            fifo->data.byte_count = fifo_len;
            /* Read the fifo data */
            rc = bmp5_get_regs(BMP5_FIFO_DATA_ADDR, fifo->data.buffer, fifo_len, dev);
        }
#if FIFOPARSE_DEBUG
        if (!rc) {
            for (i = 0; i < fifo_len; i++) {
                BMP5_LOG_ERROR("i is %d buffer[i] is %d\n", i, fifo->data.buffer[i]);
            }
        }
#endif
    } else {
        rc = SYS_ENOENT;
    }
err:
    return bmp5_check_and_return(rc, __func__);
}

/**
 * @brief This internal API parses the FIFO data frame from the fifo buffer and
 * fills compensated temperature and/or pressure data.
 *
 * @param sensor_comp   Variable used to select either temperature or
 * pressure or both while parsing the fifo frames.
 * @param fifo_buffer   FIFO buffer where the temperature or pressure or
 * both the data to be parsed.
 * @param data   Compensated temperature or pressure or both the
 * data after unpacking from fifo buffer.
 */
static void
parse_fifo_sensor_data(uint8_t sensor_comp, const uint8_t *fifo_buffer,
                       struct bmp5_data *data)
{
    /* Temporary variables to store the sensor data */
    uint32_t data_xlsb;
    uint32_t data_lsb;
    uint32_t data_msb;

    /* Store the parsed register values for temperature data */
    data_xlsb = (uint32_t)fifo_buffer[0];
    data_lsb = (uint32_t)fifo_buffer[1] << 8;
    data_msb = (uint32_t)fifo_buffer[2] << 16;

    if (sensor_comp == BMP5_TEMP) {
        /* Update compensated temperature */
        data->temperature = data_msb | data_lsb | data_xlsb;
    }
    if (sensor_comp == BMP5_PRESS) {
        /* Update compensated pressure */
        data->pressure = data_msb | data_lsb | data_xlsb;
    }
    if (sensor_comp == (BMP5_TEMP | BMP5_PRESS)) {
        data->temperature = data_msb | data_lsb | data_xlsb;
        /* Store the parsed register values for pressure data */
        data_xlsb = (uint32_t)fifo_buffer[3];
        data_lsb = (uint32_t)fifo_buffer[4] << 8;
        data_msb = (uint32_t)fifo_buffer[5] << 16;
        data->pressure = data_msb | data_lsb | data_xlsb;
    }
}

/**
 * @brief This internal API unpacks the FIFO data frame from the fifo buffer
 * and fills the byte count, compensated pressure and/or temperature data.
 *
 * @param byte_index   Byte count of fifo buffer.
 * @param fifo_buffer   FIFO buffer from where the temperature and pressure
 * frames are unpacked.
 * @param data   Compensated temperature and pressure data after
 * unpacking from fifo buffer.
 */
static void
unpack_temp_press_frame(uint16_t *byte_index, const uint8_t *fifo_buffer,
                        struct bmp5_data *data)
{
    parse_fifo_sensor_data((BMP5_PRESS | BMP5_TEMP), &fifo_buffer[*byte_index], data);
    *byte_index = *byte_index + BMP5_P_T_DATA_LEN;
}

/**
 * @brief This internal API unpacks the FIFO data frame from the fifo buffer
 * and fills the byte count and compensated temperature data.
 *
 * @param byte_index    Byte count of fifo buffer.
 * @param fifo_buffer   FIFO buffer from where the temperature frames
 * are unpacked.
 * @param data   Compensated temperature data after unpacking from
 * fifo buffer.
 */
static void
unpack_temp_frame(uint16_t *byte_index, const uint8_t *fifo_buffer,
                  struct bmp5_data *data)
{
    parse_fifo_sensor_data(BMP5_TEMP, &fifo_buffer[*byte_index], data);
    *byte_index = *byte_index + BMP5_T_DATA_LEN;
}

/**
 * @brief This internal API unpacks the FIFO data frame from the fifo buffer
 * and fills the byte count and compensated pressure data.
 *
 * @param byte_index   Byte count of fifo buffer.
 * @param fifo_buffer   FIFO buffer from where the pressure frames are
 * unpacked.
 * @param data   Compensated pressure data after unpacking from
 * fifo buffer.
 */
static void
unpack_press_frame(uint16_t *byte_index, const uint8_t *fifo_buffer,
                   struct bmp5_data *data)
{
    parse_fifo_sensor_data(BMP5_PRESS, &fifo_buffer[*byte_index], data);
    *byte_index = *byte_index + BMP5_P_DATA_LEN;
}

/**
 * @brief This internal API parse the FIFO data frame from the fifo buffer and
 * fills the byte count, compensated pressure and/or temperature data and no
 * of parsed frames.
 *
 * @param fifo   Structure instance of bmp5_fifo which stores the
 * read fifo data.
 * @param byte_index   Byte count which is incremented according to the
 * of data.
 * @param bmp5_data   Compensated pressure and/or temperature data
 * which is stored after parsing fifo buffer data.
 * @param parsed_frames   Total number of parsed frames.
 *
 * @return Result of API execution status.
 * @retval zero -> Success / -ve value -> Error
 */
static int
parse_fifo_data_frame(struct bmp5_fifo *fifo, uint16_t *byte_index,
                      struct bmp5_data *data, uint8_t *parsed_frames)
{
    uint8_t t_p_frame = 0;
#if FIFOPARSE_DEBUG
    BMP5_LOG_DEBUG("byte_index is %d\n", *byte_index);
#endif
    if (fifo->settings.temp_en && fifo->settings.press_en) {
        unpack_temp_press_frame(byte_index, fifo->data.buffer, data);
        *parsed_frames = *parsed_frames + 1;
        t_p_frame = BMP5_PRESS | BMP5_TEMP;
#if FIFOPARSE_DEBUG
        BMP5_LOG_DEBUG("parsed_frames %d\n", *parsed_frames);
        BMP5_LOG_DEBUG("BMP5_FIFO_TEMP_PRESS_FRAME\n");
#endif
    } else if (fifo->settings.temp_en) {
        unpack_temp_frame(byte_index, fifo->data.buffer, data);
        *parsed_frames = *parsed_frames + 1;
        t_p_frame = BMP5_TEMP;
#if FIFOPARSE_DEBUG
        BMP5_LOG_DEBUG("parsed_frames %d\n", *parsed_frames);
        BMP5_LOG_DEBUG("BMP5_FIFO_TEMP_FRAME\n");
#endif
    } else if (fifo->settings.press_en) {
        unpack_press_frame(byte_index, fifo->data.buffer, data);
        *parsed_frames = *parsed_frames + 1;
        t_p_frame = BMP5_PRESS;
#if FIFOPARSE_DEBUG
        BMP5_LOG_DEBUG("parsed_frames %d\n", *parsed_frames);
        BMP5_LOG_DEBUG("BMP5_FIFO_PRESS_FRAME\n");
#endif
    } else {
        fifo->data.config_err = 1;
        *byte_index = *byte_index + 1;
#if FIFOPARSE_DEBUG
        BMP5_LOG_DEBUG("unknown FIFO_FRAME\n");
#endif
    }

    return t_p_frame;
}

/**
 * @brief This API extracts the temperature and/or pressure data from the FIFO
 * data which is already read from the fifo.
 */
int
bmp5_extract_fifo_data(struct bmp5_data *data, struct bmp5_dev *dev)
{
    int rc;
    uint16_t byte_index = dev->fifo->data.start_idx;
    uint8_t parsed_frames = 0;
    uint8_t t_p_frame;

    (void)t_p_frame; /* To avoid unused variable warning */
    if (dev->fifo && data) {
        while ((parsed_frames < (dev->fifo->data.req_frames)) &&
               (byte_index < dev->fifo->data.byte_count)) {
            t_p_frame = parse_fifo_data_frame(dev->fifo, &byte_index, data,
                                              &parsed_frames);
        }
#if BMP5_DEBUG
        BMP5_LOG_DEBUG("byte_index %d\n", byte_index);
        BMP5_LOG_DEBUG("parsed_frames %d\n", parsed_frames);
#endif
        /* Check if any frames are parsed in FIFO */
        if (parsed_frames != 0) {
            /* Update the bytes parsed in the device structure */
            dev->fifo->data.start_idx = byte_index;
            dev->fifo->data.parsed_frames += parsed_frames;
        } else {
            /* No frames are there to parse. It is time to
               read the FIFO, if more frames are needed */
            dev->fifo->data.frame_not_available = true;
        }
    } else {
        rc = SYS_ENOENT;
    }

    return rc;
}

int
bmp5_run_self_test(struct bmp5 *bmp5, int *result)
{
    int rc;
    uint8_t chip_id;
    struct bmp5_data sensor_data = { 0 };
    float pressure;
    float temperature;

    rc = bmp5_get_chip_id(bmp5, &chip_id);
    if (rc) {
        *result = -1;
        rc = SYS_EINVAL;
        goto err;
    }

    if ((chip_id != BMP581_BMP580_CHIP_ID) && (chip_id != BMP585_CHIP_ID)) {
        *result = -1;
        goto err;
    } else {
        BMP5_LOG_DEBUG("self_test gets BMP5 chipID 0x%x\n", chip_id);
    }
    rc = bmp5_get_sensor_data(bmp5, &sensor_data);
    if (rc) {
        *result = -1;
        goto err;
    }
    pressure = (float)sensor_data.pressure / 10000;
    temperature = (float)sensor_data.temperature / 100;

    if ((pressure < 300) || (pressure > 1250)) {
        BMP5_LOG_ERROR("pressure data abnormal\n");
        *result = -1;
        rc = SYS_EINVAL;
        goto err;
    }
    if ((temperature < -40) || (temperature > 85)) {
        BMP5_LOG_ERROR("temperature data abnormal\n");
        *result = -1;
        rc = SYS_EINVAL;
        goto err;
    }

    *result = 0;

    return 0;
err:
    return bmp5_check_and_return(rc, __func__);
}

#if MYNEWT_VAL(BMP5_INT_ENABLE)

static void
init_interrupt(struct bmp5_int *interrupt, struct sensor_int *ints)
{
    os_error_t error;

    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);

    interrupt->active = false;
    interrupt->asleep = false;
    interrupt->ints = ints;
}

static void
undo_interrupt(struct bmp5_int *interrupt)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    interrupt->active = false;
    interrupt->asleep = false;
    OS_EXIT_CRITICAL(sr);
}

static int
wait_interrupt(struct bmp5_int *interrupt, uint8_t int_num)
{
    os_sr_t sr;
    bool wait;
    os_error_t error;

    OS_ENTER_CRITICAL(sr);

    /* Check if we did not missed interrupt */
    if (hal_gpio_read(interrupt->ints[int_num].host_pin) ==
        interrupt->ints[int_num].active) {
        OS_EXIT_CRITICAL(sr);
        return OS_OK;
    }

    if (interrupt->active) {
        interrupt->active = false;
        wait = false;
    } else {
        interrupt->asleep = true;
        wait = true;
    }
    OS_EXIT_CRITICAL(sr);

    if (wait) {
        error = os_sem_pend(&interrupt->wait, BMP5_MAX_INT_WAIT);
        if (error == OS_TIMEOUT) {
            return error;
        }
        assert(error == OS_OK);
    }

    return OS_OK;
}

static void
wake_interrupt(struct bmp5_int *interrupt)
{
    os_sr_t sr;
    bool wake;

    OS_ENTER_CRITICAL(sr);
    if (interrupt->asleep) {
        interrupt->asleep = false;
        wake = true;
    } else {
        interrupt->active = true;
        wake = false;
    }
    OS_EXIT_CRITICAL(sr);

    if (wake) {
        os_error_t error;

        error = os_sem_release(&interrupt->wait);
        assert(error == OS_OK);
    }
}

static void
bmp5_int_irq_handler(void *arg)
{
    struct sensor *sensor = arg;
    struct bmp5 *bmp5;

    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);

    if (bmp5->pdd.interrupt) {
        wake_interrupt(bmp5->pdd.interrupt);
    }

    sensor_mgr_put_interrupt_evt(sensor);
}

static int
init_intpin(struct bmp5 *bmp5, hal_gpio_irq_handler_t handler, void *arg)
{
    hal_gpio_irq_trig_t trig;
    int pin;
    int rc;

    pin = bmp5->sensor.s_itf.si_ints[0].host_pin;

    if (pin < 0) {
        BMP5_LOG_ERROR("Int pin not configured\n");
        return SYS_EINVAL;
    }

    if (bmp5->sensor.s_itf.si_ints[0].active) {
        trig = HAL_GPIO_TRIG_RISING;
    } else {
        trig = HAL_GPIO_TRIG_FALLING;
    }

    rc = hal_gpio_irq_init(pin, handler, arg, trig, HAL_GPIO_PULL_NONE);
    if (rc) {
        BMP5_LOG_ERROR("Failed to init interrupt pin %d\n", pin);
        return rc;
    }

    return 0;
}

static int
disable_interrupt(struct sensor *sensor, uint8_t int_to_disable, uint8_t int_num)
{
    struct bmp5 *bmp5;
    struct bmp5_pdd *pdd;
    struct sensor_itf *itf;
    int rc = 0;

    if (int_to_disable == 0) {
        return SYS_EINVAL;
    }
    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &bmp5->pdd;

    pdd->int_enable &= ~(int_to_disable << (int_num * 8));

    /* disable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_disable(itf->si_ints[int_num].host_pin);
        /* disable interrupt in device */
        rc = bmp5_set_int_enable(bmp5, 0, int_to_disable);
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
    struct bmp5 *bmp5;
    struct bmp5_pdd *pdd;
    struct sensor_itf *itf;
    int rc;

    if (!int_to_enable) {
        BMP5_LOG_ERROR("%s:int_to_enable is 0 \n", __func__);
        rc = SYS_EINVAL;
        goto err;
    }

    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &bmp5->pdd;

    rc = bmp5_clear_int(bmp5);
    if (rc) {
        goto err;
    }

    /* if no interrupts are currently in use enable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_enable(itf->si_ints[int_num].host_pin);

        rc = bmp5_set_int_enable(bmp5, 1, int_to_enable);
        if (rc) {
            goto err;
        }
    }

    pdd->int_enable |= (int_to_enable << (int_num * 8));

    /* enable interrupt in device */
    /* enable drdy or fifowartermark or fifofull */
    if (int_num == 0) {
    } else {
    }

    if (rc) {
        BMP5_LOG_ERROR("%s:bmp5_set_int1/int2_pin_cfg failed%d\n", rc);
        disable_interrupt(sensor, int_to_enable, int_num);
        goto err;
    }

    return 0;
err:
    return bmp5_check_and_return(rc, __func__);
}

#endif

static int
bmp5_do_report(struct sensor *sensor, sensor_type_t sensor_type,
               sensor_data_func_t data_func, void *data_arg, struct bmp5_data *data)
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

    if (sensor_type & SENSOR_TYPE_PRESSURE) {
        databuf.spd.spd_press = pressure;
        databuf.spd.spd_press_is_valid = 1;
        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.spd, SENSOR_TYPE_PRESSURE);
        if (rc) {
            goto err;
        }
    }

    if (sensor_type & SENSOR_TYPE_TEMPERATURE) {
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

int
bmp5_poll_read(struct sensor *sensor, sensor_type_t sensor_type,
               sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    struct bmp5 *bmp5;
    struct bmp5_cfg *cfg;
    int rc;
    struct bmp5_data sensor_data;

    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);
    cfg = &bmp5->cfg;

    /* If the read isn't looking for pressure data, don't do anything. */
    if ((!(sensor_type & SENSOR_TYPE_PRESSURE)) &&
        (!(sensor_type & SENSOR_TYPE_TEMPERATURE))) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (cfg->read_mode.mode != BMP5_READ_M_POLL) {
        rc = SYS_EUNKNOWN;
        goto err;
    }

    bmp5->bmp5_dev.settings.pwr_mode = BMP5_FORCED_MODE;
    rc = bmp5_set_forced_mode_with_osr(bmp5);
    if (rc) {
        goto err;
    }

    rc = bmp5_get_sensor_data(bmp5, &sensor_data);
    if (rc) {
        goto err;
    }

    rc = bmp5_do_report(sensor, sensor_type, data_func, data_arg, &sensor_data);
    if (rc) {
        goto err;
    }

err:
    bmp5_check_and_return(rc, __func__);
    /* set powermode to original */
    rc = bmp5_set_power_mode(bmp5, cfg->power_mode);
    return bmp5_check_and_return(rc, __func__);
}

int
bmp5_stream_read(struct sensor *sensor, sensor_type_t sensor_type,
                 sensor_data_func_t read_func, void *read_arg, uint32_t time_ms)
{
#if MYNEWT_VAL(BMP5_INT_ENABLE)
    struct bmp5_pdd *pdd;
#endif
    struct bmp5 *bmp5;
    struct bmp5_cfg *cfg;
    os_time_t time_ticks;
    os_time_t stop_ticks = 0;
    int rc;

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    /* FIFO object to be assigned to device structure */
    struct bmp5_fifo fifo;
    /* Pressure and temperature array of structures with maximum frame size */
    struct bmp5_data sensor_data[MYNEWT_VAL(BMP5_FIFO_CONVERTED_DATA_SIZE)];
    /* Loop Variable */
    int i;
    uint16_t frame_length;
    /* try count for polling the threshold interrupt status */
    uint16_t try_count;
    /* Enable fifo */
    fifo.settings.mode = BMP5_ENABLE;
    /* Enable Pressure sensor for fifo */
    fifo.settings.press_en = BMP5_ENABLE;
    /* Enable temperature sensor for fifo */
    fifo.settings.temp_en = BMP5_ENABLE;
    /* No subsampling for FIFO */
    fifo.settings.dec_sel = BMP5_FIFO_NO_DOWNSAMPLING;
#else
    struct bmp5_data sensor_data;

#endif
    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);
    cfg = &bmp5->cfg;

#if MYNEWT_VAL(BMP5_INT_ENABLE)
    pdd = &bmp5->pdd;
#endif

    /* If the read isn't looking for pressure or temperature data, don't do anything. */
    if ((!(sensor_type & SENSOR_TYPE_PRESSURE)) &&
        (!(sensor_type & SENSOR_TYPE_TEMPERATURE))) {
        BMP5_LOG_ERROR("unsupported sensor type for bmp5\n");
        rc = SYS_EINVAL;
        goto err;
    }

    if (cfg->read_mode.mode != BMP5_READ_M_STREAM) {
        BMP5_LOG_ERROR("mode is not stream\n");
        rc = SYS_EINVAL;
        goto err;
    }

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
#if MYNEWT_VAL(BMP5_INT_ENABLE)
    if (cfg->int_enable_type == BMP5_FIFO_THS_INT) {
        /* FIFO threshold interrupt enable */
        fifo.settings.fths_en = BMP5_ENABLE;
        fifo.data.req_frames = bmp5->bmp5_dev.fifo_threshold_level;
    } else if (cfg->int_enable_type == BMP5_FIFO_FULL_INT) {
        /* FIFO threshold interrupt enable */
        fifo.settings.ffull_en = BMP5_ENABLE;
    }
#endif
#endif

#if MYNEWT_VAL(BMP5_INT_ENABLE)
    undo_interrupt(&bmp5->intr);

    if (pdd->interrupt) {
        BMP5_LOG_ERROR("interrupt is not null\n");
        rc = SYS_EBUSY;
        goto err;
    }

    /* enable interrupt */
    pdd->interrupt = &bmp5->intr;
    /* enable and register interrupt according to interrupt type */
    rc = enable_interrupt(sensor, cfg->read_mode.int_type, cfg->read_mode.int_num);
    if (rc) {
        goto err;
    }
#else
#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    /* no interrupt feature */
    /* enable normal mode for fifo feature */
    rc = bmp5_set_normal_mode(bmp5);
    if (rc) {
        goto err;
    }
#endif
#endif

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    bmp5->bmp5_dev.fifo = &fifo;

    fifo.data.req_frames = bmp5->bmp5_dev.fifo_threshold_level;

#endif

    if (time_ms != 0) {
        if (time_ms > BMP5_MAX_STREAM_MS) {
            time_ms = BMP5_MAX_STREAM_MS;
        }
        rc = os_time_ms_to_ticks(time_ms, &time_ticks);
        if (rc) {
            goto err;
        }
        stop_ticks = os_time_get() + time_ticks;
    }

    for (;;) {

#if MYNEWT_VAL(BMP5_INT_ENABLE)
        rc = wait_interrupt(&bmp5->intr, cfg->read_mode.int_num);
        if (rc) {
            goto err;
        }
#if BMP5_DEBUG
        else {
            BMP5_LOG_DEBUG("wait_interrupt got the interrupt\n");
        }
#endif
#endif

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
        try_count = 0xFFFF;
#endif

#if MYNEWT_VAL(BMP5_INT_ENABLE)
#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
        /* till threshold level is reached in fifo and fifo interrupt happened */
        do {
            rc = bmp5_get_status(&bmp5->bmp5_dev);
            try_count--;
        } while (((bmp5->bmp5_dev.status.intr.fifo_ths == 0) &&
                  (bmp5->bmp5_dev.status.intr.fifo_full == 0)) &&
                 (try_count > 0));
#else
        rc = bmp5_get_status(&bmp5->bmp5_dev);
#endif
#else
        /* no interrupt feature */
        /* wait 2ms */
        delay_msec(2);
#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
        try_count--;
#endif
#endif

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
        if (try_count > 0) {
#if FIFOPARSE_DEBUG
            BMP5_LOG_DEBUG("%s:count:%d\n", __func__, try_count);
#endif
            rc = bmp5_get_fifo_data(&bmp5->bmp5_dev);
            rc |= bmp5_extract_fifo_data(sensor_data, &bmp5->bmp5_dev);
            if (fifo.data.frame_not_available) {
                /* no valid fifo frame in sensor */
                BMP5_LOG_ERROR("%s:fifo frames invalid %d\n", __func__, rc);
            } else {
#if BMP5_DEBUG
                BMP5_LOG_DEBUG("%s:parsed_frames:%d\n", __func__,
                               fifo.data.parsed_frames);
#endif
                frame_length = fifo.data.req_frames;
                if (frame_length > fifo.data.parsed_frames) {
                    frame_length = fifo.data.parsed_frames;
                }

                for (i = 0; i < frame_length; i++) {
                    rc = bmp5_do_report(sensor, sensor_type, read_func,
                                        read_arg, &sensor_data[i]);
                    if (rc) {
                        goto err;
                    }
                }
            }
        } else {
            BMP5_LOG_ERROR("FIFO threshold unreached\n");
            rc = SYS_EINVAL;
            goto err;
        }

#else
        if (bmp5->cfg.fifo_mode == BMP5_FIFO_M_BYPASS) {
            bmp5->bmp5_dev.settings.pwr_mode = BMP5_FORCED_MODE;
            rc = bmp5_set_forced_mode_with_osr(bmp5);
            if (rc) {
                goto err;
            }
            rc = bmp5_get_sensor_data(bmp5, &sensor_data);
            if (rc) {
                goto err;
            }

            rc = bmp5_do_report(sensor, sensor_type, read_func, read_arg, &sensor_data);
            if (rc) {
                goto err;
            }
        }
#endif

        if (time_ms != 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
            BMP5_LOG_INFO("stream tmo, increase BMP5_MAX_STREAM_MS\n");
            break;
        }
    }

err:
    bmp5_check_and_return(rc, __func__);

#if MYNEWT_VAL(BMP5_INT_ENABLE)
    /* disable interrupt */
    pdd->interrupt = NULL;
    rc = disable_interrupt(sensor, cfg->read_mode.int_type, cfg->read_mode.int_num);
    bmp5_check_and_return(rc, __func__);
#endif

    /* set powermode to original */
    rc = bmp5_set_power_mode(bmp5, cfg->power_mode);

    return bmp5_check_and_return(rc, __func__);
}

static int
bmp5_hybrid_read(struct sensor *sensor, sensor_type_t sensor_type,
                 sensor_data_func_t read_func, void *read_arg, uint32_t time_ms)
{
    int rc = 0;
    struct bmp5 *bmp5;
    struct bmp5_cfg *cfg;
    os_time_t time_ticks;
    os_time_t stop_ticks = 0;
    uint16_t try_count;

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    /* FIFO object to be assigned to device structure */
    struct bmp5_fifo fifo;
    /* Pressure and temperature array of structures with maximum frame size */
    struct bmp5_data sensor_data[MYNEWT_VAL(BMP5_FIFO_CONVERTED_DATA_SIZE)];
    /* Loop Variable */
    int i;
    uint16_t frame_length;
    /* Enable fifo */
    fifo.settings.mode = BMP5_ENABLE;
    /* Enable Pressure sensor for fifo */
    fifo.settings.press_en = BMP5_ENABLE;
    /* Enable temperature sensor for fifo */
    fifo.settings.temp_en = BMP5_ENABLE;
    /* No subsampling for FIFO */
    fifo.settings.dec_sel = BMP5_FIFO_NO_DOWNSAMPLING;
    uint16_t current_fifo_len;
#else
    struct bmp5_data sensor_data;
#endif

    /* If the read isn't looking for pressure or temperature data, don't do anything. */
    if ((!(sensor_type & SENSOR_TYPE_PRESSURE)) &&
        (!(sensor_type & SENSOR_TYPE_TEMPERATURE))) {
        BMP5_LOG_ERROR("unsupported sensor type for bmp5\n");
        return SYS_EINVAL;
    }

    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);

    cfg = &bmp5->cfg;

    if (cfg->read_mode.mode != BMP5_READ_M_HYBRID) {
        BMP5_LOG_ERROR("bmp5_hybrid_read mode is not hybrid\n");
        return SYS_EINVAL;
    }

    if (bmp5->bmp5_cfg_complete == false) {
        /* no interrupt feature */
        /* enable normal mode for fifo feature */
        rc = bmp5_set_normal_mode(bmp5);
        if (rc) {
            goto err;
        }

        rc = bmp5_fifo_flush(&bmp5->bmp5_dev);
        if (rc) {
            goto err;
        }

        rc = bmp5_set_fifo_cfg(bmp5, cfg->fifo_mode, cfg->fifo_threshold);
        if (rc) {
            goto err;
        }

#if MYNEWT_VAL(BMP5_INT_ENABLE)
        rc = bmp5_set_int_enable(bmp5, 1, bmp5->cfg.read_mode.int_type);
        if (rc) {
            goto err;
        }
        rc = bmp5_clear_int(bmp5);
        if (rc) {
            goto err;
        }
#endif
        bmp5->bmp5_cfg_complete = true;
    }

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    bmp5->bmp5_dev.fifo = &fifo;

    fifo.data.req_frames = bmp5->bmp5_dev.fifo_threshold_level;
#endif

    if (time_ms != 0) {
        if (time_ms > BMP5_MAX_STREAM_MS) {
            time_ms = BMP5_MAX_STREAM_MS;
        }
        rc = os_time_ms_to_ticks(time_ms, &time_ticks);
        if (rc) {
            goto err;
        }
        stop_ticks = os_time_get() + time_ticks;
    }

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    try_count = 0xA;

    do {
        rc = bmp5_get_status(&bmp5->bmp5_dev);
        rc |= bmp5_get_fifo_count(&current_fifo_len, &bmp5->bmp5_dev);
        delay_msec(2);
    } while (((bmp5->bmp5_dev.status.intr.fifo_ths == 0) &&
              (bmp5->bmp5_dev.status.intr.fifo_full == 0)) &&
             (try_count > 0));

    if ((rc) || (try_count == 0)) {
#if FIFOPARSE_DEBUG
        BMP5_LOG_ERROR("status %d\n", rc);
        BMP5_LOG_ERROR("try_count is %d\n", try_count);
        BMP5_LOG_ERROR("fifo length is %d\n", current_fifo_len);
#endif
        BMP5_LOG_ERROR("BMP5 STATUS READ FAILED\n");
        goto err;
    }

    rc = bmp5_get_fifo_data(&bmp5->bmp5_dev);
    if (rc) {
        BMP5_LOG_ERROR("BMP5 FIFO READ FAILED\n");
        goto err;
    }

    rc = bmp5_clear_int(bmp5);
    if (rc) {
        goto err;
    }

    rc = bmp5_extract_fifo_data(sensor_data, &bmp5->bmp5_dev);

    if (fifo.data.frame_not_available) {
        /* No valid frame read */
        BMP5_LOG_ERROR("No valid Fifo Frames %d\n", rc);
        goto err;
    } else {
#if BMP5_DEBUG
        BMP5_LOG_ERROR("parsed_frames is %d\n", fifo.data.parsed_frames);
#endif
        frame_length = fifo.data.parsed_frames;

        for (i = 0; i < frame_length; i++) {
            rc = bmp5_do_report(sensor, sensor_type, read_func, read_arg,
                                &sensor_data[i]);

            if (rc) {
                goto err;
            }
        }
    }

#else /* FIFO NOT ENABLED */
    if (bmp5->cfg.fifo_mode == BMP5_FIFO_M_BYPASS) {
        /* make data is available */
        try_count = 5;

        do {
            rc = bmp5_get_status(&bmp5->bmp5_dev);
#if MYNEWT_VAL(BMP5_INT_ENABLE)
            if (!rc && bmp5->bmp5_dev.status.intr.drdy) {
                break;
            }
#else
            if (!rc) {
                break;
            }
#endif
            delay_msec(2);
#if FIFOPARSE_DEBUG
            BMP5_LOG_ERROR("status %d\n", rc);
#endif
        } while (--try_count > 0);

        rc = bmp5_get_sensor_data(bmp5, &sensor_data);
        if (rc) {
            goto err;
        }

        rc = bmp5_do_report(sensor, sensor_type, read_func, read_arg, &sensor_data);
        if (rc) {
            goto err;
        }
    }
#endif /* #if MYNEWT_VAL(BMP5_FIFO_ENABLE) */

    if (time_ms != 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
        BMP5_LOG_INFO("stream tmo\n");
        BMP5_LOG_INFO("Increase BMP5_MAX_STREAM_MS to extend"
                      "stream time duration\n");
        goto err;
    }

    return rc;

err:
    bmp5_check_and_return(rc, __func__);
    /* reset device */
    rc = bmp5_set_power_mode(bmp5, cfg->power_mode);
    bmp5->bmp5_cfg_complete = false;
    return bmp5_check_and_return(rc, __func__);

} /* bmp5_hybrid_read */

static int
bmp5_sensor_read(struct sensor *sensor, sensor_type_t type,
                 sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int rc;
    const struct bmp5_cfg *cfg;
    struct bmp5 *bmp5;
#if BMP5_DEBUG
    BMP5_LOG_ERROR("%s:entered\n");
#endif
    /* If the read isn't looking for pressure data, don't do anything. */
    if ((!(type & SENSOR_TYPE_PRESSURE)) && (!(type & SENSOR_TYPE_TEMPERATURE))) {
        rc = SYS_EINVAL;
        BMP5_LOG_ERROR("%s:unsupported sensor type\n");
        goto err;
    }

    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);
    cfg = &bmp5->cfg;

    if (cfg->read_mode.mode == BMP5_READ_M_POLL) {
        rc = bmp5_poll_read(sensor, type, data_func, data_arg, timeout);
    } else if (cfg->read_mode.mode == BMP5_READ_M_STREAM) {
        rc = bmp5_stream_read(sensor, type, data_func, data_arg, timeout);
    } else { /* hybrid mode */
        rc = bmp5_hybrid_read(sensor, type, data_func, data_arg, timeout);
    }

    return 0;
err:
    return bmp5_check_and_return(rc, __func__);
}

static int
bmp5_sensor_set_notification(struct sensor *sensor, sensor_event_type_t event)
{
#if MYNEWT_VAL(BMP5_INT_ENABLE)

    struct bmp5 *bmp5;
    struct bmp5_pdd *pdd;
    int rc;

    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);
    pdd = &bmp5->pdd;

    rc = enable_interrupt(sensor, bmp5->cfg.int_enable_type, MYNEWT_VAL(BMP5_INT_NUM));
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
bmp5_sensor_unset_notification(struct sensor *sensor, sensor_event_type_t event)
{

#if MYNEWT_VAL(BMP5_INT_ENABLE)
    struct bmp5 *bmp5;

    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);

    bmp5->pdd.notify_ctx.snec_evtype &= ~event;

    return disable_interrupt(sensor, bmp5->cfg.int_enable_type,
                             MYNEWT_VAL(BMP5_INT_NUM));
#else
    return 0;
#endif
}

static int
bmp5_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct bmp5 *bmp5;

    bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);

    return bmp5_config(bmp5, (struct bmp5_cfg *)cfg);
}

static int
bmp5_sensor_handle_interrupt(struct sensor *sensor)
{
#if MYNEWT_VAL(BMP5_INT_ENABLE)
    struct bmp5 *bmp5 = (struct bmp5 *)SENSOR_GET_DEVICE(sensor);
#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
#endif
    uint8_t int_status_all;
    int rc;
#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    /* FIFO object to be assigned to device structure */
    struct bmp5_fifo fifo;
    bmp5->bmp5_dev.fifo = &fifo;
#endif

    BMP5_LOG_ERROR("%s:entered\n");

    rc = bmp5_get_status(&bmp5->bmp5_dev);
    if (rc) {
        BMP5_LOG_ERROR("%s:status err=0x%02x\n", rc);
        goto err;
    }

#if MYNEWT_VAL(BMP5_FIFO_ENABLE)
    if ((bmp5->cfg.int_enable_type == BMP5_FIFO_THS_INT) ||
        (bmp5->cfg.int_enable_type == BMP5_FIFO_FULL_INT)) {
        rc = bmp5_fifo_flush(&bmp5->bmp5_dev);
        if (rc) {
            goto err;
        }
    }
#endif

    int_status_all = bmp5->bmp5_dev.status.intr.fifo_ths |
                     bmp5->bmp5_dev.status.intr.fifo_full |
                     bmp5->bmp5_dev.status.intr.drdy;

    if (int_status_all == 0) {
        BMP5_LOG_ERROR("No int\n");
        rc = SYS_EINVAL;
        goto err;
    }
#if CLEAR_INT_AFTER_ISR
    rc = bmp5_clear_int(bmp5);
    if (rc) {
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
bmp5_sensor_get_config(struct sensor *sensor, sensor_type_t type,
                       struct sensor_cfg *cfg)
{
    int rc;
    (void)sensor;

    if ((type & (SENSOR_TYPE_PRESSURE | SENSOR_TYPE_TEMPERATURE)) == 0) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;

    return 0;
err:
    return rc;
}

int
bmp5_init(struct os_dev *dev, void *arg)
{
    struct bmp5 *bmp5;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

#if BMP5_DEBUG
    BMP5_LOG_ERROR("%s:entered\n");
#endif
    bmp5 = (struct bmp5 *)dev;

    bmp5->cfg.mask = SENSOR_TYPE_ALL;

    sensor = &bmp5->sensor;

    /* Initialise the stats entry */
    rc = stats_init(STATS_HDR(bmp5->stats),
                    STATS_SIZE_INIT_PARMS(bmp5->stats, STATS_SIZE_32),
                    STATS_NAME_INIT_PARMS(bmp5_stat_section));
    SYSINIT_PANIC_ASSERT(!rc);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(bmp5->stats));
    SYSINIT_PANIC_ASSERT(!rc);

    rc = sensor_init(sensor, dev);
    if (rc) {
        goto err;
    }

    /* Add the light driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_TEMPERATURE | SENSOR_TYPE_PRESSURE,
                           (struct sensor_driver *)&g_bmp5_sensor_driver);

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

#if MYNEWT_VAL(BMP5_INT_ENABLE)
    init_interrupt(&bmp5->intr, bmp5->sensor.s_itf.si_ints);

    bmp5->pdd.notify_ctx.snec_sensor = sensor;
    bmp5->pdd.interrupt = NULL;

    rc = init_intpin(bmp5, bmp5_int_irq_handler, sensor);
    if (rc) {
        BMP5_LOG_ERROR("init_intpin failed\n");
        return rc;
    }

#endif

#if BMP5_DEBUG
    BMP5_LOG_ERROR("%s:exited\n");
#endif
    return 0;
err:
    return rc;
}

int
bmp5_config(struct bmp5 *bmp5, struct bmp5_cfg *cfg)
{
    int rc;
    uint8_t chip_id;

    rc = bmp5_itf_init(bmp5);
    if (rc) {
        goto err;
    }

    rc = bmp5_get_chip_id(bmp5, &chip_id);
    if (rc) {
        goto err;
    }

    if ((chip_id != BMP581_BMP580_CHIP_ID) && (chip_id != BMP585_CHIP_ID)) {
        rc = SYS_EINVAL;
        BMP5_LOG_ERROR("%s:BMP5 chipID failed 0x%x\n", __func__, chip_id);
        goto err;
    } else {
        BMP5_LOG_ERROR("%s:gets BMP5 chipID 0x%x\n", __func__, chip_id);
    }

    /* Configure sensor in standby mode */
    rc = bmp5_set_power_mode(bmp5, BMP5_STANDBY_MODE);
    if (rc) {
        goto err;
    }

    delay_msec(2);

    rc = bmp5_set_int_pp_od(bmp5, cfg->int_pp_od);
    if (rc) {
        goto err;
    }
    bmp5->cfg.int_pp_od = cfg->int_pp_od;

    rc = bmp5_set_int_mode(bmp5, cfg->int_mode);
    if (rc) {
        goto err;
    }
    bmp5->cfg.int_mode = cfg->int_mode;

    rc = bmp5_set_int_active_pol(bmp5, cfg->int_active_pol);
    if (rc) {
        goto err;
    }
    bmp5->cfg.int_active_pol = cfg->int_active_pol;

    rc = bmp5_set_filter_cfg(bmp5, cfg->filter_press_osr, cfg->filter_temp_osr);
    if (rc) {
        goto err;
    }

    /* filter.press_os filter.temp_os */
    bmp5->cfg.filter_press_osr = cfg->filter_press_osr;
    bmp5->cfg.filter_temp_osr = cfg->filter_temp_osr;

    rc = bmp5_set_rate(bmp5, cfg->rate);
    if (rc) {
        goto err;
    }

    bmp5->cfg.rate = cfg->rate;

    rc = bmp5_set_fifo_cfg(bmp5, cfg->fifo_mode, cfg->fifo_threshold);
    if (rc) {
        goto err;
    }

    bmp5->cfg.fifo_mode = cfg->fifo_mode;
    bmp5->cfg.fifo_threshold = cfg->fifo_threshold;

    bmp5->cfg.int_enable_type = cfg->int_enable_type;

    rc = bmp5_set_power_mode(bmp5, cfg->power_mode);
    if (rc) {
        goto err;
    }

    bmp5->cfg.power_mode = cfg->power_mode;


    rc = sensor_set_type_mask(&(bmp5->sensor), cfg->mask);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(BMP5_CLI)
    bmp5_shell_init();
#endif

    bmp5->cfg.read_mode.int_type = cfg->read_mode.int_type;
    bmp5->cfg.read_mode.int_num = cfg->read_mode.int_num;
    bmp5->cfg.read_mode.mode = cfg->read_mode.mode;

    bmp5->cfg.mask = cfg->mask;

#if BMP5_DEBUG
    BMP5_LOG_ERROR("%s:exited\n", __func__);
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

    bmp5_init((struct os_dev *)bnode, itf);
}

int
bmp5_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                           const struct bus_i2c_node_cfg *i2c_cfg,
                           struct sensor_itf *sensor_itf)
{
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };

    sensor_itf->si_dev = &node->bnode.odev;
    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    return bus_i2c_node_create(name, node, i2c_cfg, sensor_itf);
}
#endif
