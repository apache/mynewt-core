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

#include <os/os_dev.h>
#include <assert.h>
#include <defs/error.h>

#if MYNEWT_VAL(LSM303DLHC_OFB)
#include <lsm303dlhc/lsm303dlhc.h>
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

/* Driver definitions */
#if MYNEWT_VAL(LSM303DLHC_OFB)
static struct lsm303dlhc lsm303dlhc;
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

#if MYNEWT_VAL(SPI_0_MASTER) && MYNEWT_VAL(BME280_OFB)
static struct sensor_itf spi_0_itf_bme = {
    .si_type = SENSOR_ITF_SPI,
    .si_num = 0,
    .si_cspin = 3
};
#endif

#if MYNEWT_VAL(I2C_0) && MYNEWT_VAL(LSM303DLHC_OFB)
static struct sensor_itf i2c_0_itf_lsm = {
    .si_type = SENSOR_ITF_I2C,
    .si_num  = 0,
    .si_addr = 0
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

    lsmcfg.mask = SENSOR_TYPE_LINEAR_ACCEL|
                  SENSOR_TYPE_MAGNETIC_FIELD;

    rc = lsm303dlhc_config((struct lsm303dlhc *) dev, &lsmcfg);

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

/* Sensor device creation */
void
sensor_dev_create(void)
{
    int rc;

    (void)rc;
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

}
