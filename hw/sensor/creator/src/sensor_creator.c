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
#include <string.h>
#include "os/mynewt.h"

#if MYNEWT_VAL(DRV2605_OFB)
#include "hal/hal_gpio.h"
#include <drv2605/drv2605.h>
#endif
#if MYNEWT_VAL(LSM303DLHC_OFB)
#include <lsm303dlhc/lsm303dlhc.h>
#endif
#if MYNEWT_VAL(MPU6050_OFB)
#include <mpu6050/mpu6050.h>
#endif
#if MYNEWT_VAL(BNO055_OFB)
#include <bno055/bno055.h>
#endif
#if MYNEWT_VAL(TSL2561_OFB)
#include <tsl2561/tsl2561.h>
#endif
#if MYNEWT_VAL(TCS34725_OFB)
#include <tcs34725/tcs34725.h>
#endif
#if MYNEWT_VAL(BME280_OFB)
#include <bme280/bme280.h>
#endif

#if MYNEWT_VAL(MS5837_OFB)
#include <ms5837/ms5837.h>
#endif

#if MYNEWT_VAL(MS5840_OFB)
#include <ms5840/ms5840.h>
#endif

#if MYNEWT_VAL(BMP280_OFB)
#include <bmp280/bmp280.h>
#endif

#if MYNEWT_VAL(BMA253_OFB)
#include <bma253/bma253.h>
#endif

#if MYNEWT_VAL(BMA2XX_OFB)
#include <bma2xx/bma2xx.h>
#endif


#if MYNEWT_VAL(ADXL345_OFB)
#include <adxl345/adxl345.h>
#endif

#if MYNEWT_VAL(LPS33HW_OFB)
#include <lps33hw/lps33hw.h>
#endif

#if MYNEWT_VAL(LIS2DW12_OFB)
#include <lis2dw12/lis2dw12.h>
#endif

/* Driver definitions */
#if MYNEWT_VAL(DRV2605_OFB)
static struct drv2605 drv2605;
#endif

#if MYNEWT_VAL(LSM303DLHC_OFB)
static struct lsm303dlhc lsm303dlhc;
#endif

#if MYNEWT_VAL(MPU6050_OFB)
static struct mpu6050 mpu6050;
#endif

#if MYNEWT_VAL(BNO055_OFB)
static struct bno055 bno055;
#endif

#if MYNEWT_VAL(TSL2561_OFB)
static struct tsl2561 tsl2561;
#endif

#if MYNEWT_VAL(TCS34725_OFB)
static struct tcs34725 tcs34725;
#endif

#if MYNEWT_VAL(BME280_OFB)
static struct bme280 bme280;
#endif

#if MYNEWT_VAL(MS5837_OFB)
static struct ms5837 ms5837;
#endif

#if MYNEWT_VAL(MS5840_OFB)
static struct ms5840 ms5840;
#endif

#if MYNEWT_VAL(BMP280_OFB)
static struct bmp280 bmp280;
#endif

#if MYNEWT_VAL(BMA253_OFB)
static struct bma253 bma253;
#endif

#if MYNEWT_VAL(BMA2XX_OFB)
static struct bma2xx bma2xx;
#endif

#if MYNEWT_VAL(ADXL345_OFB)
static struct adxl345 adxl345;
#endif

#if MYNEWT_VAL(LPS33HW_OFB)
static struct lps33hw lps33hw;
#endif

#if MYNEWT_VAL(LIS2DW12_OFB)
static struct lis2dw12 lis2dw12;
#endif

/**
 * If a UART sensor needs to be created, interface is defined in
 * the following way
 *
 * #if MYNEWT_VAL(UART_0)
 * static const struct sensor_itf uart_0_itf = {
 *   .si_type = SENSOR_ITF_UART,
 *   .si_num = 0,
 * };
 * #endif
 *
 * #if MYNEWT_VAL(UART_1)
 * static struct sensor_itf uart_1_itf = {
 *    .si_type = SENSOR_ITF_UART,
 *    .si_num = 1,
 *};
 *#endif
 */

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(BMP280_OFB)
static struct sensor_itf i2c_0_itf_bmp = {
    .si_type = SENSOR_ITF_I2C,
    .si_num = 0,
    .si_addr = BMP280_DFLT_I2C_ADDR
};
#endif

#if MYNEWT_VAL(SPI_0_MASTER) && MYNEWT_VAL(BME280_OFB)
static struct sensor_itf spi_0_itf_bme = {
    .si_type = SENSOR_ITF_SPI,
    .si_num = 0,
    .si_cs_pin = 3
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(DRV2605_OFB)
static struct sensor_itf i2c_0_itf_drv = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    .si_addr = MYNEWT_VAL(DRV2605_SHELL_ITF_ADDR),
    .si_cs_pin = MYNEWT_VAL(DRV2605_EN_PIN)
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(LSM303DLHC_OFB)
static struct sensor_itf i2c_0_itf_lsm = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    .si_addr = 0
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(MPU6050_OFB)
static struct sensor_itf i2c_0_itf_mpu = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    .si_addr = MPU6050_I2C_ADDR
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(BNO055_OFB)
static struct sensor_itf i2c_0_itf_bno = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    /* HW I2C address for the BNO055 */
    .si_addr = 0x28
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(TSL2561_OFB)
static struct sensor_itf i2c_0_itf_tsl = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    /*  I2C address for the TSL2561 (0x29, 0x39 or 0x49) */
    .si_addr = 0x39
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(TCS34725_OFB)
static struct sensor_itf i2c_0_itf_tcs = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    /* HW I2C address for the TCS34725 */
    .si_addr = 0x29
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(MS5837_OFB)
static struct sensor_itf i2c_0_itf_ms = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    /* HW I2C address for the MS5837 */
    .si_addr = 0x76
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(MS5840_OFB)
static struct sensor_itf i2c_0_itf_ms = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    /* HW I2C address for the MS5840 */
    .si_addr = 0x76
};
#endif


#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(BMA253_OFB)
static struct sensor_itf i2c_0_itf_lis = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    .si_addr = 0x18,
    .si_ints = {
        { 26, MYNEWT_VAL(BMA2XX_INT_PIN_DEVICE),
            MYNEWT_VAL(BMA2XX_INT_CFG_ACTIVE)},
        { 25, MYNEWT_VAL(BMA2XX_INT2_PIN_DEVICE),
            MYNEWT_VAL(BMA2XX_INT_CFG_ACTIVE)}
    },
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(BMA2XX_OFB)
static struct sensor_itf spi2c_0_itf_bma2xx = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    .si_addr = 0x18,
    .si_ints = {
        { 26, MYNEWT_VAL(BMA2XX_INT_PIN_DEVICE),
            MYNEWT_VAL(BMA2XX_INT_CFG_ACTIVE)},
        { 25, MYNEWT_VAL(BMA2XX_INT2_PIN_DEVICE),
            MYNEWT_VAL(BMA2XX_INT_CFG_ACTIVE)}
    },
};
#endif
#if MYNEWT_VAL(SPI_0_MASTER) && MYNEWT_VAL(BMA2XX_OFB)
//TODO:  Make INT pin nums configurable.  Leaving hardcoded
//to handle multiple bma2xx sensor interface examples
static struct sensor_itf spi2c_0_itf_bma2xx = {
    .si_type = SENSOR_ITF_SPI,
    .si_num = 0,
    .si_cs_pin = 21,
    .si_ints = {
        { 26, MYNEWT_VAL(BMA2XX_INT_PIN_DEVICE),
            MYNEWT_VAL(BMA2XX_INT_CFG_ACTIVE)},
        { 25, MYNEWT_VAL(BMA2XX_INT2_PIN_DEVICE),
            MYNEWT_VAL(BMA2XX_INT_CFG_ACTIVE)}
    },
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(ADXL345_OFB)
static struct sensor_itf i2c_0_itf_adxl = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    .si_addr = 0x1D,
    .si_ints = {
       { MYNEWT_VAL(ADXL345_INT_PIN_HOST), MYNEWT_VAL(ADXL345_INT_PIN_DEVICE),
         MYNEWT_VAL(ADXL345_INT_CFG_ACTIVE)}}
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(LPS33HW_OFB)
static struct sensor_itf i2c_0_itf_lps = {
    .si_type = MYNEWT_VAL(LPS33HW_SHELL_ITF_TYPE),
    .si_num  = MYNEWT_VAL(LPS33HW_SHELL_ITF_NUM),
    .si_addr = MYNEWT_VAL(LPS33HW_SHELL_ITF_ADDR)
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(LIS2DW12_OFB)
static struct sensor_itf i2c_0_itf_lis2dw12 = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    .si_addr = 0x18,
    .si_ints = {
        { MYNEWT_VAL(LIS2DW12_INT1_PIN_HOST), MYNEWT_VAL(LIS2DW12_INT1_PIN_DEVICE),
          MYNEWT_VAL(LIS2DW12_INT1_CFG_ACTIVE)}}
};
#endif

/**
 * MS5837 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(MS5837_OFB)
static int
config_ms5837_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct ms5837_cfg mscfg;

    dev = (struct os_dev *) os_dev_open("ms5837_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    memset(&mscfg, 0, sizeof(mscfg));


    mscfg.mc_s_temp_res_osr  = MS5837_RES_OSR_256;
    mscfg.mc_s_press_res_osr = MS5837_RES_OSR_256;
    mscfg.mc_s_mask = SENSOR_TYPE_AMBIENT_TEMPERATURE|
                      SENSOR_TYPE_PRESSURE;

    rc = ms5837_config((struct ms5837 *)dev, &mscfg);

    os_dev_close(dev);
    return rc;
}
#endif

/**
 * MS5840 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(MS5840_OFB)
static int
config_ms5840_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct ms5840_cfg mscfg;

    dev = (struct os_dev *) os_dev_open("ms5840_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    memset(&mscfg, 0, sizeof(mscfg));


    mscfg.mc_s_temp_res_osr  = MS5840_RES_OSR_256;
    mscfg.mc_s_press_res_osr = MS5840_RES_OSR_256;
    mscfg.mc_s_mask = SENSOR_TYPE_AMBIENT_TEMPERATURE|
                      SENSOR_TYPE_PRESSURE;

    rc = ms5840_config((struct ms5840 *)dev, &mscfg);

    os_dev_close(dev);
    return rc;
}
#endif


/* Sensor default configuration used by the creator package */

/**
 * BME280 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(BME280_OFB)
static int
config_bme280_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct bme280_cfg bmecfg;

    dev = (struct os_dev *) os_dev_open("bme280_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    memset(&bmecfg, 0, sizeof(bmecfg));

    bmecfg.bc_mode = BME280_MODE_NORMAL;
    bmecfg.bc_iir = BME280_FILTER_X16;
    bmecfg.bc_sby_dur = BME280_STANDBY_MS_0_5;
    bmecfg.bc_boc[0].boc_type = SENSOR_TYPE_RELATIVE_HUMIDITY;
    bmecfg.bc_boc[1].boc_type = SENSOR_TYPE_PRESSURE;
    bmecfg.bc_boc[2].boc_type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    bmecfg.bc_boc[0].boc_oversample = BME280_SAMPLING_X1;
    bmecfg.bc_boc[1].boc_oversample = BME280_SAMPLING_X16;
    bmecfg.bc_boc[2].boc_oversample = BME280_SAMPLING_X2;
    bmecfg.bc_s_mask = SENSOR_TYPE_AMBIENT_TEMPERATURE|
                       SENSOR_TYPE_PRESSURE|
                       SENSOR_TYPE_RELATIVE_HUMIDITY;

    rc = bme280_config((struct bme280 *)dev, &bmecfg);

    os_dev_close(dev);
    return rc;
}
#endif

/**
 * BMP280 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(BMP280_OFB)
static int
config_bmp280_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct bmp280_cfg bmpcfg;

    dev = (struct os_dev *) os_dev_open("bmp280_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    memset(&bmpcfg, 0, sizeof(bmpcfg));

    bmpcfg.bc_mode = BMP280_MODE_NORMAL;
    bmpcfg.bc_iir = BMP280_FILTER_X16;
    bmpcfg.bc_sby_dur = BMP280_STANDBY_MS_0_5;
    bmpcfg.bc_boc[0].boc_type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    bmpcfg.bc_boc[1].boc_type = SENSOR_TYPE_PRESSURE;
    bmpcfg.bc_boc[0].boc_oversample = BMP280_SAMPLING_X2;
    bmpcfg.bc_boc[1].boc_oversample = BMP280_SAMPLING_X16;
    bmpcfg.bc_s_mask = SENSOR_TYPE_AMBIENT_TEMPERATURE|
                       SENSOR_TYPE_PRESSURE;

    rc = bmp280_config((struct bmp280 *)dev, &bmpcfg);

    os_dev_close(dev);
    return rc;
}

#endif
/**
 * TCS34725 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(TCS34725_OFB)
static int
config_tcs34725_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct tcs34725_cfg tcscfg;

    dev = (struct os_dev *) os_dev_open("tcs34725_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    /* Gain set to 16X and Inetgration time set to 24ms */
    tcscfg.gain = TCS34725_GAIN_16X;
    tcscfg.integration_time = TCS34725_INTEGRATIONTIME_24MS;
    tcscfg.int_enable = 1;
    tcscfg.mask = SENSOR_TYPE_COLOR;

    rc = tcs34725_config((struct tcs34725 *)dev, &tcscfg);

    os_dev_close(dev);
    return rc;
}
#endif

/**
 * TSL2561 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(TSL2561_OFB)
static int
config_tsl2561_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct tsl2561_cfg tslcfg;

    dev = (struct os_dev *) os_dev_open("tsl2561_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    /* Gain set to 1X and Inetgration time set to 13ms */
    tslcfg.gain = TSL2561_LIGHT_GAIN_1X;
    tslcfg.integration_time = TSL2561_LIGHT_ITIME_13MS;
    tslcfg.mask = SENSOR_TYPE_LIGHT;

    rc = tsl2561_config((struct tsl2561 *)dev, &tslcfg);

    os_dev_close(dev);
    return rc;
}
#endif

/**
 * DRV2605 Actuator default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(DRV2605_OFB)
static int
config_drv2605_actuator(void)
{
    int rc;
    struct os_dev *dev;
    struct drv2605_cfg cfg = {0};

    dev = (struct os_dev *) os_dev_open("drv2605_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    rc = drv2605_config((struct drv2605 *) dev, &cfg);

    os_dev_close(dev);
    return rc;
}
#endif

/**
 * LSM303DLHC Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(LSM303DLHC_OFB)
static int
config_lsm303dlhc_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct lsm303dlhc_cfg lsmcfg;

    dev = (struct os_dev *) os_dev_open("lsm303dlhc_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    /* read once per sec.  API should take this value in ms. */
    lsmcfg.accel_rate = LSM303DLHC_ACCEL_RATE_1;
    lsmcfg.accel_range = LSM303DLHC_ACCEL_RANGE_2;
    /* Device I2C addr for accelerometer */
    lsmcfg.acc_addr = LSM303DLHC_ADDR_ACCEL;
    /* Device I2C addr for magnetometer */
    lsmcfg.mag_addr = LSM303DLHC_ADDR_MAG;
    /* Set default mag gain to +/-1.3 gauss */
    lsmcfg.mag_gain = LSM303DLHC_MAG_GAIN_1_3;
    /* Set default mag sample rate to 15Hz */
    lsmcfg.mag_rate = LSM303DLHC_MAG_RATE_15;

    lsmcfg.mask = SENSOR_TYPE_ACCELEROMETER|
                  SENSOR_TYPE_MAGNETIC_FIELD;

    rc = lsm303dlhc_config((struct lsm303dlhc *) dev, &lsmcfg);

    os_dev_close(dev);
    return rc;
}
#endif

/**
 * MPU6050 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(MPU6050_OFB)
static int
config_mpu6050_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct mpu6050_cfg mpucfg;

    dev = (struct os_dev *) os_dev_open("mpu6050_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    mpucfg.accel_range = MPU6050_ACCEL_RANGE_4;
    mpucfg.gyro_range = MPU6050_GYRO_RANGE_500;
    mpucfg.clock_source = MPU6050_CLK_GYRO_X;
    mpucfg.sample_rate_div = 39; /* Sample Rate = Gyroscope Output Rate /
            (1 + sample_rate_div) */
    mpucfg.lpf_cfg = 3; /* See data sheet */
    mpucfg.int_enable = 0;
    mpucfg.int_cfg = MPU6050_INT_LATCH_EN | MPU6050_INT_RD_CLEAR;
    mpucfg.mask = SENSOR_TYPE_ACCELEROMETER | SENSOR_TYPE_GYROSCOPE;

    rc = mpu6050_config((struct mpu6050 *) dev, &mpucfg);

    os_dev_close(dev);
    return rc;
}
#endif

/**
 * BNO055 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(BNO055_OFB)
static int
config_bno055_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct bno055_cfg bcfg;

    dev = (struct os_dev *) os_dev_open("bno055_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    bcfg.bc_units = BNO055_ACC_UNIT_MS2   | BNO055_ANGRATE_UNIT_DPS |
                    BNO055_EULER_UNIT_DEG | BNO055_TEMP_UNIT_DEGC   |
                    BNO055_DO_FORMAT_ANDROID;

    bcfg.bc_opr_mode = BNO055_OPR_MODE_NDOF;
    bcfg.bc_pwr_mode = BNO055_PWR_MODE_NORMAL;
    bcfg.bc_acc_bw = BNO055_ACC_CFG_BW_125HZ;
    bcfg.bc_acc_range =  BNO055_ACC_CFG_RNG_16G;
    bcfg.bc_use_ext_xtal = 1;
    bcfg.bc_mask = SENSOR_TYPE_ACCELEROMETER|
                   SENSOR_TYPE_MAGNETIC_FIELD|
                   SENSOR_TYPE_GYROSCOPE|
                   SENSOR_TYPE_EULER|
                   SENSOR_TYPE_GRAVITY|
                   SENSOR_TYPE_LINEAR_ACCEL|
                   SENSOR_TYPE_ROTATION_VECTOR;

    rc = bno055_config((struct bno055 *) dev, &bcfg);

    os_dev_close(dev);
    return rc;
}
#endif

#if MYNEWT_VAL(BMA253_OFB)
/**
 * BMA253 sensor default configuration
 *
 * @return 0 on success, non-zero on failure
 */
int
config_bma253_sensor(void)
{
    struct os_dev * dev;
    struct bma253_cfg cfg = {0};
    int rc;

    dev = os_dev_open("bma253_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    cfg.low_g_delay_ms = BMA253_LOW_G_DELAY_MS_DEFAULT;
    cfg.high_g_delay_ms = BMA253_HIGH_G_DELAY_MS_DEFAULT;
    cfg.g_range = BMA253_G_RANGE_2;
    cfg.filter_bandwidth = BMA253_FILTER_BANDWIDTH_1000_HZ;
    cfg.use_unfiltered_data = false;
    cfg.tap_quiet = BMA253_TAP_QUIET_30_MS;
    cfg.tap_shock = BMA253_TAP_SHOCK_50_MS;
    cfg.d_tap_window = BMA253_D_TAP_WINDOW_250_MS;
    cfg.tap_wake_samples = BMA253_TAP_WAKE_SAMPLES_2;
    cfg.tap_thresh_g = 1.0;
    cfg.offset_x_g = 0.0;
    cfg.offset_y_g = 0.0;
    cfg.offset_z_g = 0.0;
    cfg.power_mode = BMA253_POWER_MODE_NORMAL;
    cfg.sleep_duration = BMA253_SLEEP_DURATION_0_5_MS;
    cfg.sensor_mask = SENSOR_TYPE_ACCELEROMETER;
    cfg.read_mode = BMA253_READ_M_POLL;

    rc = bma253_config((struct bma253 *)dev, &cfg);
    assert(rc == 0);

    os_dev_close(dev);

    return 0;
}
#endif

/**
 * ADXL345 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(ADXL345_OFB)
static int
config_adxl345_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct adxl345_cfg cfg;

    dev = (struct os_dev *) os_dev_open("adxl345_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    cfg.power_mode = ADXL345_POWER_MEASURE;
    cfg.low_power_enable = 0;
    cfg.accel_range = ADXL345_ACCEL_RANGE_4;
    cfg.sample_rate = ADXL345_RATE_12_5_HZ;

    cfg.offset_x = 0;
    cfg.offset_y = 0;
    cfg.offset_z = 0;

    cfg.tap_cfg.threshold = 0x30; /* 3g */
    cfg.tap_cfg.duration = 0x10;  /* 10 ms */
    cfg.tap_cfg.latency = 0x10;   /* 20 ms */
    cfg.tap_cfg.window = 0x80;    /* 160 ms */
    cfg.tap_cfg.x_enable = 1;
    cfg.tap_cfg.y_enable = 1;
    cfg.tap_cfg.z_enable = 1;
    cfg.tap_cfg.suppress = 0;

    cfg.freefall_threshold = 0x07; /* 440mg */
    cfg.freefall_time = 0x14; /* 100ms */

    cfg.mask = SENSOR_TYPE_ACCELEROMETER;

    rc = adxl345_config((struct adxl345 *) dev, &cfg);
    assert(rc == 0);
#endif

/*
 * LPS33HW Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(LPS33HW_OFB)
static int
config_lps33hw_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct lps33hw_cfg cfg;

    cfg.mask = SENSOR_TYPE_PRESSURE | SENSOR_TYPE_TEMPERATURE;
    cfg.data_rate = LPS33HW_1HZ;
    cfg.lpf = LPS33HW_LPF_DISABLED;
    cfg.int_cfg.pin = 0;
    cfg.int_cfg.data_rdy = 0;
    cfg.int_cfg.pressure_low = 0;
    cfg.int_cfg.pressure_high = 0;
    cfg.int_cfg.active_low = 0;
    cfg.int_cfg.open_drain = 0;
    cfg.int_cfg.latched = 0;

    dev = (struct os_dev *) os_dev_open("lps33hw_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    rc = lps33hw_config((struct lps33hw *) dev, &cfg);

    os_dev_close(dev);
    return rc;
}
#endif

/**
 * LIS2DW12 Sensor default configuration used by the creator package
 *
 * @return 0 on success, non-zero on failure
 */
#if MYNEWT_VAL(LIS2DW12_OFB)
static int
config_lis2dw12_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct lis2dw12_cfg cfg;

    dev = (struct os_dev *) os_dev_open("lis2dw12_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    cfg.rate = LIS2DW12_DATA_RATE_200HZ;
    cfg.fs = LIS2DW12_FS_2G;

    cfg.offset_x = 0;
    cfg.offset_y = 0;
    cfg.offset_z = 0;
    cfg.offset_weight = 0;
    cfg.offset_en = 0;

    cfg.filter_bw = LIS2DW12_FILTER_BW_ODR_DIV_2;
    cfg.high_pass = 0;
    
    cfg.tap.en_x = 1;
    cfg.tap.en_y = 1;
    cfg.tap.en_z = 1;
    cfg.tap.en_4d = 0;
    cfg.tap.ths_6d = LIS2DW12_6D_THS_80_DEG;
    cfg.tap.tap_priority = LIS2DW12_TAP_PRIOR_XYZ;
    cfg.tap.tap_ths_x = 0x3;
    cfg.tap.tap_ths_y = 0x3;
    cfg.tap.tap_ths_z = 0x3;
    cfg.tap.latency = 8; /* 640ms */
    cfg.tap.quiet = 0; /* 10ms */
    cfg.tap.shock = 3; /* 120ms */

    cfg.double_tap_event_enable = 0;

    cfg.freefall_dur = 6;
    cfg.freefall_ths = 3; /* ~312mg */

    cfg.int1_pin_cfg = 0;
    cfg.int2_pin_cfg = 0;
    cfg.int_enable = 0;

    cfg.int_pp_od = 0;
    cfg.int_latched = 0;
    cfg.int_active_low = 0;
    cfg.slp_mode = 0;
    cfg.self_test_mode = LIS2DW12_ST_MODE_DISABLE;

    cfg.fifo_mode = LIS2DW12_FIFO_M_BYPASS;
    cfg.fifo_threshold = 32;

    cfg.wake_up_ths = 0;
    cfg.wake_up_dur = 0;
    cfg.sleep_duration = 0;

    cfg.stationary_detection_enable = 0;

    cfg.power_mode = LIS2DW12_PM_HIGH_PERF;
    cfg.inactivity_sleep_enable = 0;
    cfg.low_noise_enable = 1;
    
    cfg.read_mode.mode = LIS2DW12_READ_M_POLL;

    cfg.mask = SENSOR_TYPE_ACCELEROMETER;

    rc = lis2dw12_config((struct lis2dw12 *) dev, &cfg);
    assert(rc == 0);

    os_dev_close(dev);
    return rc;
}
#endif

#if MYNEWT_VAL(BMA2XX_OFB)
/**
 * BMA2XX sensor default configuration
 *
 * @return 0 on success, non-zero on failure
 */
int
config_bma2xx_sensor(void)
{
    struct os_dev * dev;
    struct bma2xx_cfg cfg;
    int rc;

    dev = os_dev_open("bma2xx_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    cfg.model = BMA2XX_BMA280;
    cfg.low_g_delay_ms = BMA2XX_LOW_G_DELAY_MS_DEFAULT;
    cfg.high_g_delay_ms = BMA2XX_HIGH_G_DELAY_MS_DEFAULT;
    cfg.g_range = BMA2XX_G_RANGE_2;
    cfg.filter_bandwidth = BMA2XX_FILTER_BANDWIDTH_500_HZ;
    cfg.use_unfiltered_data = false;
    cfg.tap_quiet = BMA2XX_TAP_QUIET_30_MS;
    cfg.tap_shock = BMA2XX_TAP_SHOCK_50_MS;
    cfg.d_tap_window = BMA2XX_D_TAP_WINDOW_250_MS;
    cfg.tap_wake_samples = BMA2XX_TAP_WAKE_SAMPLES_16;
    cfg.tap_thresh_g = 1.0;
    cfg.offset_x_g = 0.0;
    cfg.offset_y_g = 0.0;
    cfg.offset_z_g = 0.0;
    cfg.orient_blocking = BMA2XX_ORIENT_BLOCKING_NONE;
    cfg.orient_mode = BMA2XX_ORIENT_MODE_SYMMETRICAL;
    cfg.power_mode = BMA2XX_POWER_MODE_NORMAL;
    cfg.sleep_duration = BMA2XX_SLEEP_DURATION_0_5_MS;
    cfg.sensor_mask = SENSOR_TYPE_ACCELEROMETER;

    rc = bma2xx_config((struct bma2xx *)dev, &cfg);
    assert(rc == 0);

    os_dev_close(dev);

    return 0;
}
#endif

/* Sensor device creation */
void
sensor_dev_create(void)
{
    int rc;

    (void)rc;
#if MYNEWT_VAL(DRV2605_OFB)
    rc = hal_gpio_init_out(MYNEWT_VAL(DRV2605_EN_PIN), 1);
    assert(rc == 0);

    rc = os_dev_create((struct os_dev *) &drv2605, "drv2605_0",
      OS_DEV_INIT_PRIMARY, 0, drv2605_init, (void *)&i2c_0_itf_drv);
    assert(rc == 0);

    rc = config_drv2605_actuator();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(LSM303DLHC_OFB)
    /* Since this sensor has multiple I2C addreses,
     * 0x1E for accelerometer and 0x19 for magnetometer,
     * they are made part of the config. Not setting the address in the sensor
     * interface makes it take the address either from the driver or
     * from the config, however teh develeoper would like to deal with it.
     */
    rc = os_dev_create((struct os_dev *) &lsm303dlhc, "lsm303dlhc_0",
      OS_DEV_INIT_PRIMARY, 0, lsm303dlhc_init, (void *)&i2c_0_itf_lsm);
    assert(rc == 0);

    rc = config_lsm303dlhc_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(MPU6050_OFB)
    rc = os_dev_create((struct os_dev *) &mpu6050, "mpu6050_0",
      OS_DEV_INIT_PRIMARY, 0, mpu6050_init, (void *)&i2c_0_itf_mpu);
    assert(rc == 0);

    rc = config_mpu6050_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(BNO055_OFB)
    rc = os_dev_create((struct os_dev *) &bno055, "bno055_0",
      OS_DEV_INIT_PRIMARY, 0, bno055_init, (void *)&i2c_0_itf_bno);
    assert(rc == 0);

    rc = config_bno055_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TSL2561_OFB)
    rc = os_dev_create((struct os_dev *) &tsl2561, "tsl2561_0",
      OS_DEV_INIT_PRIMARY, 0, tsl2561_init, (void *)&i2c_0_itf_tsl);
    assert(rc == 0);

    rc = config_tsl2561_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TCS34725_OFB)
    rc = os_dev_create((struct os_dev *) &tcs34725, "tcs34725_0",
      OS_DEV_INIT_PRIMARY, 0, tcs34725_init, (void *)&i2c_0_itf_tcs);
    assert(rc == 0);

    rc = config_tcs34725_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(BME280_OFB)
    rc = os_dev_create((struct os_dev *) &bme280, "bme280_0",
      OS_DEV_INIT_PRIMARY, 0, bme280_init, (void *)&spi_0_itf_bme);
    assert(rc == 0);

    rc = config_bme280_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(MS5837_OFB)
    rc = os_dev_create((struct os_dev *) &ms5837, "ms5837_0",
      OS_DEV_INIT_PRIMARY, 0, ms5837_init, (void *)&i2c_0_itf_ms);
    assert(rc == 0);

    rc = config_ms5837_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(MS5840_OFB)
    rc = os_dev_create((struct os_dev *) &ms5840, "ms5840_0",
      OS_DEV_INIT_PRIMARY, 0, ms5840_init, (void *)&i2c_0_itf_ms);
    assert(rc == 0);

    rc = config_ms5840_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(BMP280_OFB)
    rc = os_dev_create((struct os_dev *) &bmp280, "bmp280_0",
      OS_DEV_INIT_PRIMARY, 0, bmp280_init, (void *)&i2c_0_itf_bmp);
    assert(rc == 0);

    rc = config_bmp280_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(BMA253_OFB)
    rc = os_dev_create((struct os_dev *)&bma253, "bma253_0",
      OS_DEV_INIT_PRIMARY, 0, bma253_init, &i2c_0_itf_lis);
    assert(rc == 0);

    rc = config_bma253_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(BMA2XX_OFB)
rc = os_dev_create((struct os_dev *)&bma2xx, "bma2xx_0",
  OS_DEV_INIT_PRIMARY, 0, bma2xx_init, &spi2c_0_itf_bma2xx);
assert(rc == 0);

rc = config_bma2xx_sensor();
assert(rc == 0);
#endif

#if MYNEWT_VAL(ADXL345_OFB)
    rc = os_dev_create((struct os_dev *) &adxl345, "adxl345_0",
      OS_DEV_INIT_PRIMARY, 0, adxl345_init, (void *)&i2c_0_itf_adxl);
    assert(rc == 0);

    rc = config_adxl345_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(LPS33HW_OFB)
    rc = os_dev_create((struct os_dev *) &lps33hw, "lps33hw_0",
      OS_DEV_INIT_PRIMARY, 0, lps33hw_init, (void *)&i2c_0_itf_lps);
    assert(rc == 0);

    rc = config_lps33hw_sensor();
    assert(rc == 0);
#endif

#if MYNEWT_VAL(LIS2DW12_OFB)
    rc = os_dev_create((struct os_dev *) &lis2dw12, "lis2dw12_0",
      OS_DEV_INIT_PRIMARY, 0, lis2dw12_init, (void *)&i2c_0_itf_lis2dw12);
    assert(rc == 0);

    rc = config_lis2dw12_sensor();
    assert(rc == 0);
#endif

}
