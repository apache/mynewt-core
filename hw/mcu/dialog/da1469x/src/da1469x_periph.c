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
#include "syscfg/syscfg.h"
#include "hal/hal_timer.h"
#include "os/os_cputime.h"
#include "mcu/da1469x_hal.h"

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/bus.h"
#if MYNEWT_VAL(I2C_0) || MYNEWT_VAL(I2C_1)
#include "bus/drivers/i2c_hal.h"
#endif
#else
#if MYNEWT_VAL(I2C_0) || MYNEWT_VAL(I2C_1)
#include "hal/hal_i2c.h"
#endif
#endif

#if MYNEWT_VAL(I2C_0)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_i2c_dev_cfg i2c0_cfg = {
    .i2c_num = 0,
    .pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
};
static struct bus_i2c_dev i2c0_bus;
#else
static const struct da1469x_hal_i2c_cfg hal_i2c0_cfg = {
    .pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
    .frequency = MYNEWT_VAL(I2C_0_FREQ_KHZ),
};
#endif
#endif
#if MYNEWT_VAL(I2C_1)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_i2c_dev_cfg i2c1_cfg = {
    .i2c_num = 1,
    .pin_sda = MYNEWT_VAL(I2C_1_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_1_PIN_SCL),
};
static struct bus_i2c_dev i2c1_bus;
#else
static const struct da1469x_hal_i2c_cfg hal_i2c1_cfg = {
    .pin_sda = MYNEWT_VAL(I2C_1_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_1_PIN_SCL),
    .frequency = MYNEWT_VAL(I2C_1_FREQ_KHZ),
};
#endif
#endif

static void
da1469x_periph_create_timers(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_2)
    rc = hal_timer_init(2, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

static void
da1469x_periph_create_i2c(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(I2C_0)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_i2c_hal_dev_create("i2c0", &i2c0_bus,
                                (struct bus_i2c_dev_cfg *)&i2c0_cfg);
    assert(rc == 0);
#else
    rc = hal_i2c_init(0, (void *)&hal_i2c0_cfg);
    assert(rc == 0);
#endif
#endif

#if MYNEWT_VAL(I2C_1)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_i2c_hal_dev_create("i2c1", &i2c1_bus,
                                (struct bus_i2c_dev_cfg *)&i2c1_cfg);
    assert(rc == 0);
#else
    rc = hal_i2c_init(1, (void *)&hal_i2c1_cfg);
    assert(rc == 0);
#endif
#endif
}

void
da1469x_periph_create(void)
{
    da1469x_periph_create_timers();
//    da1469x_periph_create_adc();
//    da1469x_periph_create_pwm();
//    da1469x_periph_create_trng();
//    da1469x_periph_create_uart();
    da1469x_periph_create_i2c();
//    da1469x_periph_create_spi();
}
