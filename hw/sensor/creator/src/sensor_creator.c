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

#if MYNEWT_VAL(UART_0)
static const struct sensor_itf uart_0_itf = {
    .si_type = SENSOR_ITF_UART,
    .si_num = 0,
};
#endif

#if MYNEWT_VAL(UART_1)
static const struct sensor_itf uart_1_itf = {
    .si_type = SENSOR_ITF_UART,
    .si_num = 1,
};
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
static const struct sensor_itf spi_0_itf = {
    .si_type = SENSOR_ITF_SPI,
    .si_num = 0,
    .si_cspin = 3
};
#endif

#if MYNEWT_VAL(I2C_0)
static const struct sensor_itf i2c_0_itf = {
    .si_type = SENSOR_ITF_I2C,
    .si_num = 0,
};
#endif

void
sensor_dev_create(void)
{
    int rc;

    (void)rc;
#if MYNEWT_VAL(LSM303DLHC_OFB)
    rc = os_dev_create((struct os_dev *) &lsm303dlhc, "accel0",
      OS_DEV_INIT_PRIMARY, 0, lsm303dlhc_init, (void *)&i2c_0_itf);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(BNO055_OFB)
    rc = os_dev_create((struct os_dev *) &bno055, "accel1",
      OS_DEV_INIT_PRIMARY, 0, bno055_init, (void *)&i2c_0_itf);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TSL2561_OFB)
    rc = os_dev_create((struct os_dev *) &tsl2561, "light0",
      OS_DEV_INIT_PRIMARY, 0, tsl2561_init, (void *)&i2c_0_itf);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TCS34725_OFB)
    rc = os_dev_create((struct os_dev *) &tcs34725, "color0",
      OS_DEV_INIT_PRIMARY, 0, tcs34725_init, (void *)&i2c_0_itf);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(BME280_OFB)
    rc = os_dev_create((struct os_dev *) &bme280, "bme280",
      OS_DEV_INIT_PRIMARY, 0, bme280_init, (void *)&spi_0_itf);
    assert(rc == 0);
#endif

}
