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

#include <assert.h>
#include <math.h>
#include <string.h>

#include "os/mynewt.h"
#include "bma253/bma253.h"
#include "bma253_priv.h"
#include "hal/hal_gpio.h"
#include "hal/hal_i2c.h"

#if MYNEWT_VAL(BMA253_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(BMA253_LOG)
static struct log bma253_log;
#define LOG_MODULE_BMA253 (253)
#define BMA253_ERROR(...) LOG_ERROR(&bma253_log, LOG_MODULE_BMA253, __VA_ARGS__)
#define BMA253_INFO(...)  LOG_INFO(&bma253_log, LOG_MODULE_BMA253, __VA_ARGS__)
#else
#define BMA253_ERROR(...)
#define BMA253_INFO(...)
#endif

#define BMA253_NOTIFY_MASK  0x01
#define BMA253_READ_MASK    0x02

static void
delay_msec(uint32_t delay)
{
    delay = (delay * OS_TICKS_PER_SEC) / 1000 + 1;
    os_time_delay(delay);
}

#if MYNEWT_VAL(BMA253_INT_ENABLE)
static void
init_interrupt(struct bma253_int * interrupt, struct sensor_int *ints)
{
    os_error_t error;

    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);

    interrupt->active = false;
    interrupt->asleep = false;
    interrupt->ints = ints;
}

static void
undo_interrupt(struct bma253_int * interrupt)
{
    OS_ENTER_CRITICAL(interrupt->lock);
    interrupt->active = false;
    interrupt->asleep = false;
    OS_EXIT_CRITICAL(interrupt->lock);
}

static void
wait_interrupt(struct bma253_int * interrupt, enum bma253_int_num int_num)
{
    bool wait;

    OS_ENTER_CRITICAL(interrupt->lock);

    /* Check if we did not missed interrupt */
    if (hal_gpio_read(interrupt->ints[int_num].host_pin) ==
                                            interrupt->ints[int_num].active) {
        OS_EXIT_CRITICAL(interrupt->lock);
        return;
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
        os_error_t error;

        error = os_sem_pend(&interrupt->wait, -1);
        assert(error == OS_OK);
    }
}

static void
wake_interrupt(struct bma253_int * interrupt)
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
interrupt_handler(void * arg)
{
    struct sensor *sensor = arg;
    struct bma253 *bma253;

    bma253 = (struct bma253 *)SENSOR_GET_DEVICE(sensor);

    if (bma253->pdd.interrupt) {
        wake_interrupt(bma253->pdd.interrupt);
    }

    sensor_mgr_put_interrupt_evt(sensor);
}
#endif

static int
get_register(const struct bma253 * bma253,
             uint8_t addr,
             uint8_t * data)
{
    const struct sensor_itf * itf;
    struct hal_i2c_master_data oper;
    int rc;

    itf = SENSOR_GET_ITF(&bma253->sensor);

    oper.address = itf->si_addr;
    oper.len     = 1;
    oper.buffer  = &addr;

    rc = hal_i2c_master_write(itf->si_num, &oper,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc != 0) {
        BMA253_ERROR("I2C access failed at address 0x%02X\n", addr);
        return rc;
    }

    oper.address = itf->si_addr;
    oper.len     = 1;
    oper.buffer  = data;

    rc = hal_i2c_master_read(itf->si_num, &oper,
                             OS_TICKS_PER_SEC / 10, 1);
    if (rc != 0) {
        BMA253_ERROR("I2C read failed at address 0x%02X single byte\n",
                     addr);
        return rc;
    }

    return 0;
}

static int
get_registers(const struct bma253 * bma253,
              uint8_t addr,
              uint8_t * data,
              uint8_t size)
{
    const struct sensor_itf * itf;
    struct hal_i2c_master_data oper;
    int rc;

    itf = SENSOR_GET_ITF(&bma253->sensor);

    oper.address = itf->si_addr;
    oper.len     = 1;
    oper.buffer  = &addr;

    rc = hal_i2c_master_write(itf->si_num, &oper,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc != 0) {
        BMA253_ERROR("I2C access failed at address 0x%02X\n",
                     addr);
        return rc;
    }

    oper.address = itf->si_addr;
    oper.len     = size;
    oper.buffer  = data;

    rc = hal_i2c_master_read(itf->si_num, &oper,
                             OS_TICKS_PER_SEC / 10, 1);
    if (rc != 0) {
        BMA253_ERROR("I2C read failed at address 0x%02X length %u\n",
                     addr, size);
        return rc;
    }

    return 0;
}

static int
set_register(const struct bma253 * bma253,
             uint8_t addr,
             uint8_t data)
{
    const struct sensor_itf * itf;
    uint8_t tuple[2];
    struct hal_i2c_master_data oper;
    int rc;

    itf = SENSOR_GET_ITF(&bma253->sensor);

    tuple[0] = addr;
    tuple[1] = data;

    oper.address = itf->si_addr;
    oper.len     = 2;
    oper.buffer  = tuple;

    rc = hal_i2c_master_write(itf->si_num, &oper,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc != 0) {
        BMA253_ERROR("I2C write failed at address 0x%02X single byte\n",
                     addr);
        return rc;
    }

    switch (bma253->power) {
    case BMA253_POWER_MODE_SUSPEND:
    case BMA253_POWER_MODE_LPM_1:
        delay_msec(1);
        break;
    default:
        break;
    }

    return 0;
}

int
bma253_get_chip_id(const struct bma253 * bma253,
                   uint8_t * chip_id)
{
    return get_register(bma253, REG_ADDR_BGW_CHIPID, chip_id);
}

static void
compute_accel_data(struct accel_data * accel_data,
                   const uint8_t * raw_data,
                   float accel_scale)
{
    int16_t raw_accel;

    raw_accel = (raw_data[0] >> 4) | (raw_data[1] << 4);
    raw_accel <<= 4;
    raw_accel >>= 4;

    accel_data->accel_g = (float)raw_accel * accel_scale;
    accel_data->new_data = raw_data[0] & 0x01;
}

int
bma253_get_accel(const struct bma253 * bma253,
                 enum bma253_g_range g_range,
                 enum axis axis,
                 struct accel_data * accel_data)
{
    float accel_scale;
    uint8_t base_addr;
    uint8_t data[2];
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        accel_scale = 0.00098;
        break;
    case BMA253_G_RANGE_4:
        accel_scale = 0.00195;
        break;
    case BMA253_G_RANGE_8:
        accel_scale = 0.00391;
        break;
    case BMA253_G_RANGE_16:
        accel_scale = 0.00781;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (axis) {
    case AXIS_X:
        base_addr = REG_ADDR_ACCD_X_LSB;
        break;
    case AXIS_Y:
        base_addr = REG_ADDR_ACCD_Y_LSB;
        break;
    case AXIS_Z:
        base_addr = REG_ADDR_ACCD_Z_LSB;
        break;
    default:
        return SYS_EINVAL;
    }

    rc = get_registers(bma253, base_addr,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    compute_accel_data(accel_data, data, accel_scale);

    return 0;
}

int
bma253_get_temp(const struct bma253 * bma253,
                float * temp_c)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_ACCD_TEMP, &data);
    if (rc != 0) {
        return rc;
    }

    *temp_c = (float)(int8_t)data * 0.5 + 23.0;

    return 0;
}

static void
quad_to_axis_trigger(struct axis_trigger * axis_trigger,
                     uint8_t quad_bits,
                     const char * name_bits)
{
    axis_trigger->sign = (quad_bits >> 3) & 0x01;
    switch (quad_bits & 0x07) {
    default:
        BMA253_ERROR("unknown %s quad bits 0x%02X\n",
                     name_bits, quad_bits);
    case 0x00:
        axis_trigger->axis = -1;
        axis_trigger->axis_known = false;
        break;
    case 0x01:
        axis_trigger->axis = AXIS_X;
        axis_trigger->axis_known = true;
        break;
    case 0x02:
        axis_trigger->axis = AXIS_Y;
        axis_trigger->axis_known = true;
        break;
    case 0x03:
        axis_trigger->axis = AXIS_Z;
        axis_trigger->axis_known = true;
        break;
    }
}

int
bma253_get_int_status(const struct bma253 * bma253,
                      struct int_status * int_status)
{
    uint8_t data[4];
    int rc;

    rc = get_registers(bma253, REG_ADDR_INT_STATUS_0,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    int_status->flat_int_active = data[0] & 0x80;
    int_status->orient_int_active = data[0] & 0x40;
    int_status->s_tap_int_active = data[0] & 0x20;
    int_status->d_tap_int_active = data[0] & 0x10;
    int_status->slow_no_mot_int_active = data[0] & 0x08;
    int_status->slope_int_active = data[0] & 0x04;
    int_status->high_g_int_active = data[0] & 0x02;
    int_status->low_g_int_active = data[0] & 0x01;
    int_status->data_int_active = data[1] & 0x80;
    int_status->fifo_wmark_int_active = data[1] & 0x40;
    int_status->fifo_full_int_active = data[1] & 0x20;
    quad_to_axis_trigger(&int_status->tap_trigger,
                 (data[2] >> 4) & 0x0F, "tap");
    quad_to_axis_trigger(&int_status->slope_trigger,
                 (data[2] >> 0) & 0x0F, "slope");
    int_status->device_is_flat = data[3] & 0x80;
    int_status->device_is_down = data[3] & 0x40;
    int_status->device_orientation = (data[3] >> 4) & 0x03;
    quad_to_axis_trigger(&int_status->high_g_trigger,
                 (data[3] >> 0) & 0x0F, "high_g");

    return 0;
}

int
bma253_get_fifo_status(const struct bma253 * bma253,
                       bool * overrun,
                       uint8_t * frame_counter)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_FIFO_STATUS, &data);
    if (rc != 0) {
        return rc;
    }

    *overrun = data & 0x80;
    *frame_counter = data & 0x7F;

    return 0;
}

int
bma253_get_g_range(const struct bma253 * bma253,
                   enum bma253_g_range * g_range)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_PMU_RANGE, &data);
    if (rc != 0) {
        return rc;
    }

    switch (data & 0x0F) {
    default:
        BMA253_ERROR("unknown PMU_RANGE reg value 0x%02X\n", data);
        *g_range = BMA253_G_RANGE_16;
        break;
    case 0x03:
        *g_range = BMA253_G_RANGE_2;
        break;
    case 0x05:
        *g_range = BMA253_G_RANGE_4;
        break;
    case 0x08:
        *g_range = BMA253_G_RANGE_8;
        break;
    case 0x0C:
        *g_range = BMA253_G_RANGE_16;
        break;
    }

    return 0;
}

int
bma253_set_g_range(const struct bma253 * bma253,
                   enum bma253_g_range g_range)
{
    uint8_t data;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        data = 0x03;
        break;
    case BMA253_G_RANGE_4:
        data = 0x05;
        break;
    case BMA253_G_RANGE_8:
        data = 0x08;
        break;
    case BMA253_G_RANGE_16:
        data = 0x0C;
        break;
    default:
        return SYS_EINVAL;
    }

    return set_register(bma253, REG_ADDR_PMU_RANGE, data);
}

int
bma253_get_filter_bandwidth(const struct bma253 * bma253,
                            enum bma253_filter_bandwidth * filter_bandwidth)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_PMU_BW, &data);
    if (rc != 0) {
        return rc;
    }

    switch (data & 0x1F) {
    case 0x00 ... 0x08:
        *filter_bandwidth = BMA253_FILTER_BANDWIDTH_7_81_HZ;
        break;
    case 0x09:
        *filter_bandwidth = BMA253_FILTER_BANDWIDTH_15_63_HZ;
        break;
    case 0x0A:
        *filter_bandwidth = BMA253_FILTER_BANDWIDTH_31_25_HZ;
        break;
    case 0x0B:
        *filter_bandwidth = BMA253_FILTER_BANDWIDTH_62_5_HZ;
        break;
    case 0x0C:
        *filter_bandwidth = BMA253_FILTER_BANDWIDTH_125_HZ;
        break;
    case 0x0D:
        *filter_bandwidth = BMA253_FILTER_BANDWIDTH_250_HZ;
        break;
    case 0x0E:
        *filter_bandwidth = BMA253_FILTER_BANDWIDTH_500_HZ;
        break;
    case 0x0F ... 0x1F:
        *filter_bandwidth = BMA253_FILTER_BANDWIDTH_1000_HZ;
        break;
    }

    return 0;
}

int
bma253_set_filter_bandwidth(const struct bma253 * bma253,
                            enum bma253_filter_bandwidth filter_bandwidth)
{
    uint8_t data;

    switch (filter_bandwidth) {
    case BMA253_FILTER_BANDWIDTH_7_81_HZ:
        data = 0x08;
        break;
    case BMA253_FILTER_BANDWIDTH_15_63_HZ:
        data = 0x09;
        break;
    case BMA253_FILTER_BANDWIDTH_31_25_HZ:
        data = 0x0A;
        break;
    case BMA253_FILTER_BANDWIDTH_62_5_HZ:
        data = 0x0B;
        break;
    case BMA253_FILTER_BANDWIDTH_125_HZ:
        data = 0x0C;
        break;
    case BMA253_FILTER_BANDWIDTH_250_HZ:
        data = 0x0D;
        break;
    case BMA253_FILTER_BANDWIDTH_500_HZ:
        data = 0x0E;
        break;
    case BMA253_FILTER_BANDWIDTH_1000_HZ:
        data = 0x0F;
        break;
    default:
        return SYS_EINVAL;
    }

    return set_register(bma253, REG_ADDR_PMU_BW, data);
}

int
bma253_get_power_settings(const struct bma253 * bma253,
                          struct power_settings * power_settings)
{
    uint8_t data[2];
    int rc;

    rc = get_registers(bma253, REG_ADDR_PMU_LPW,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    switch ((data[0] >> 5) & 0x07) {
    default:
        BMA253_ERROR("unknown PMU_LPW reg value 0x%02X\n", data[0]);
        power_settings->power_mode = BMA253_POWER_MODE_NORMAL;
        break;
    case 0x00:
        power_settings->power_mode = BMA253_POWER_MODE_NORMAL;
        break;
    case 0x01:
        power_settings->power_mode = BMA253_POWER_MODE_DEEP_SUSPEND;
        break;
    case 0x02:
        if ((data[1] & 0x40) == 0) {
            power_settings->power_mode = BMA253_POWER_MODE_LPM_1;
        } else {
            power_settings->power_mode = BMA253_POWER_MODE_LPM_2;
        }
        break;
    case 0x04:
        if ((data[1] & 0x40) == 0) {
            power_settings->power_mode = BMA253_POWER_MODE_SUSPEND;
        } else {
            power_settings->power_mode = BMA253_POWER_MODE_STANDBY;
        }
        break;
    }

    switch ((data[0] >> 1) & 0x0F) {
    case 0x00 ... 0x05:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_0_5_MS;
        break;
    case 0x06:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_1_MS;
        break;
    case 0x07:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_2_MS;
        break;
    case 0x08:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_4_MS;
        break;
    case 0x09:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_6_MS;
        break;
    case 0x0A:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_10_MS;
        break;
    case 0x0B:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_25_MS;
        break;
    case 0x0C:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_50_MS;
        break;
    case 0x0D:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_100_MS;
        break;
    case 0x0E:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_500_MS;
        break;
    case 0x0F:
        power_settings->sleep_duration = BMA253_SLEEP_DURATION_1_S;
        break;
    }

    if ((data[1] & 0x20) != 0) {
        power_settings->sleep_timer = SLEEP_TIMER_EQUIDISTANT_SAMPLING;
    } else {
        power_settings->sleep_timer = SLEEP_TIMER_EVENT_DRIVEN;
    }

    return 0;
}

int
bma253_set_power_settings(const struct bma253 * bma253,
                          const struct power_settings * power_settings)
{
    uint8_t data[2];
    int rc;

    data[0] = 0;
    data[1] = 0;

    switch (power_settings->power_mode) {
    case BMA253_POWER_MODE_NORMAL:
        data[0] |= 0x00 << 5;
        break;
    case BMA253_POWER_MODE_DEEP_SUSPEND:
        data[0] |= 0x01 << 5;
        break;
    case BMA253_POWER_MODE_SUSPEND:
        data[0] |= 0x04 << 5;
        data[1] |= 0x00 << 6;
        break;
    case BMA253_POWER_MODE_STANDBY:
        data[0] |= 0x04 << 5;
        data[1] |= 0x01 << 6;
        break;
    case BMA253_POWER_MODE_LPM_1:
        data[0] |= 0x02 << 5;
        data[1] |= 0x00 << 6;
        break;
    case BMA253_POWER_MODE_LPM_2:
        data[0] |= 0x02 << 5;
        data[1] |= 0x01 << 6;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (power_settings->sleep_duration) {
    case BMA253_SLEEP_DURATION_0_5_MS:
        data[0] |= 0x05 << 1;
        break;
    case BMA253_SLEEP_DURATION_1_MS:
        data[0] |= 0x06 << 1;
        break;
    case BMA253_SLEEP_DURATION_2_MS:
        data[0] |= 0x07 << 1;
        break;
    case BMA253_SLEEP_DURATION_4_MS:
        data[0] |= 0x08 << 1;
        break;
    case BMA253_SLEEP_DURATION_6_MS:
        data[0] |= 0x09 << 1;
        break;
    case BMA253_SLEEP_DURATION_10_MS:
        data[0] |= 0x0A << 1;
        break;
    case BMA253_SLEEP_DURATION_25_MS:
        data[0] |= 0x0B << 1;
        break;
    case BMA253_SLEEP_DURATION_50_MS:
        data[0] |= 0x0C << 1;
        break;
    case BMA253_SLEEP_DURATION_100_MS:
        data[0] |= 0x0D << 1;
        break;
    case BMA253_SLEEP_DURATION_500_MS:
        data[0] |= 0x0E << 1;
        break;
    case BMA253_SLEEP_DURATION_1_S:
        data[0] |= 0x0F << 1;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (power_settings->sleep_timer) {
    case SLEEP_TIMER_EVENT_DRIVEN:
        data[1] |= 0x00 << 5;
        break;
    case SLEEP_TIMER_EQUIDISTANT_SAMPLING:
        data[1] |= 0x01 << 5;
        break;
    default:
        return SYS_EINVAL;
    }

    rc = set_register(bma253, REG_ADDR_PMU_LOW_POWER, data[1]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_PMU_LPW, data[0]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_data_acquisition(const struct bma253 * bma253,
                            bool * unfiltered_reg_data,
                            bool * disable_reg_shadow)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_ACCD_HBW, &data);
    if (rc != 0) {
        return rc;
    }

    *unfiltered_reg_data = data & 0x80;
    *disable_reg_shadow = data & 0x40;

    return 0;
}

int
bma253_set_data_acquisition(const struct bma253 * bma253,
                            bool unfiltered_reg_data,
                            bool disable_reg_shadow)
{
    uint8_t data;

    data = (unfiltered_reg_data << 7) |
           (disable_reg_shadow << 6);

    return set_register(bma253, REG_ADDR_ACCD_HBW, data);
}

int
bma253_set_softreset(const struct bma253 * bma253)
{
    int rc;

    rc = set_register(bma253, REG_ADDR_BGW_SOFTRESET, REG_VALUE_SOFT_RESET);
    if (rc != 0) {
        return rc;
    }

    delay_msec(2);

    return 0;
}

int
bma253_get_int_enable(const struct bma253 * bma253,
                      struct int_enable * int_enable)
{
    uint8_t data[3];
    int rc;

    rc = get_registers(bma253, REG_ADDR_INT_EN_0,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    int_enable->flat_int_enable = data[0] & 0x80;
    int_enable->orient_int_enable = data[0] & 0x40;
    int_enable->s_tap_int_enable = data[0] & 0x20;
    int_enable->d_tap_int_enable = data[0] & 0x10;
    int_enable->slope_z_int_enable = data[0] & 0x04;
    int_enable->slope_y_int_enable = data[0] & 0x02;
    int_enable->slope_x_int_enable = data[0] & 0x01;
    int_enable->fifo_wmark_int_enable = data[1] & 0x40;
    int_enable->fifo_full_int_enable = data[1] & 0x20;
    int_enable->data_int_enable = data[1] & 0x10;
    int_enable->low_g_int_enable = data[1] & 0x08;
    int_enable->high_g_z_int_enable = data[1] & 0x04;
    int_enable->high_g_y_int_enable = data[1] & 0x02;
    int_enable->high_g_x_int_enable = data[1] & 0x01;
    int_enable->no_motion_select = data[2] & 0x08;
    int_enable->slow_no_mot_z_int_enable = data[2] & 0x04;
    int_enable->slow_no_mot_y_int_enable = data[2] & 0x02;
    int_enable->slow_no_mot_x_int_enable = data[2] & 0x01;

    return 0;
}

int
bma253_set_int_enable(const struct bma253 * bma253,
                      const struct int_enable * int_enable)
{
    uint8_t data[3];
    int rc;

    data[0] = (int_enable->flat_int_enable << 7) |
              (int_enable->orient_int_enable << 6) |
              (int_enable->s_tap_int_enable << 5) |
              (int_enable->d_tap_int_enable << 4) |
              (int_enable->slope_z_int_enable << 2) |
              (int_enable->slope_y_int_enable << 1) |
              (int_enable->slope_x_int_enable << 0);

    data[1] = (int_enable->fifo_wmark_int_enable << 6) |
              (int_enable->fifo_full_int_enable << 5) |
              (int_enable->data_int_enable << 4) |
              (int_enable->low_g_int_enable << 3) |
              (int_enable->high_g_z_int_enable << 2) |
              (int_enable->high_g_y_int_enable << 1) |
              (int_enable->high_g_x_int_enable << 0);

    data[2] = (int_enable->no_motion_select << 3) |
              (int_enable->slow_no_mot_z_int_enable << 2) |
              (int_enable->slow_no_mot_y_int_enable << 1) |
              (int_enable->slow_no_mot_x_int_enable << 0);

    rc = set_register(bma253, REG_ADDR_INT_EN_0, data[0]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_EN_1, data[1]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_EN_2, data[2]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_int_routes(const struct bma253 * bma253,
                      struct int_routes * int_routes)
{
    uint8_t data[3];
    int rc;

    rc = get_registers(bma253, REG_ADDR_INT_MAP_0,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    int_routes->flat_int_route = INT_ROUTE_NONE;
    if ((data[0] & 0x80) != 0) {
        int_routes->flat_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[2] & 0x80) != 0) {
        int_routes->flat_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->orient_int_route = INT_ROUTE_NONE;
    if ((data[0] & 0x40) != 0) {
        int_routes->orient_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[2] & 0x40) != 0) {
        int_routes->orient_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->s_tap_int_route = INT_ROUTE_NONE;
    if ((data[0] & 0x20) != 0) {
        int_routes->s_tap_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[2] & 0x20) != 0) {
        int_routes->s_tap_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->d_tap_int_route = INT_ROUTE_NONE;
    if ((data[0] & 0x10) != 0) {
        int_routes->d_tap_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[2] & 0x10) != 0) {
        int_routes->d_tap_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->slow_no_mot_int_route = INT_ROUTE_NONE;
    if ((data[0] & 0x08) != 0) {
        int_routes->slow_no_mot_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[2] & 0x08) != 0) {
        int_routes->slow_no_mot_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->slope_int_route = INT_ROUTE_NONE;
    if ((data[0] & 0x04) != 0) {
        int_routes->slope_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[2] & 0x04) != 0) {
        int_routes->slope_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->high_g_int_route = INT_ROUTE_NONE;
    if ((data[0] & 0x02) != 0) {
        int_routes->high_g_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[2] & 0x02) != 0) {
        int_routes->high_g_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->low_g_int_route = INT_ROUTE_NONE;
    if ((data[0] & 0x01) != 0) {
        int_routes->low_g_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[2] & 0x01) != 0) {
        int_routes->low_g_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->fifo_wmark_int_route = INT_ROUTE_NONE;
    if ((data[1] & 0x02) != 0) {
        int_routes->fifo_wmark_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[1] & 0x40) != 0) {
        int_routes->fifo_wmark_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->fifo_full_int_route = INT_ROUTE_NONE;
    if ((data[1] & 0x04) != 0) {
        int_routes->fifo_full_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[1] & 0x20) != 0) {
        int_routes->fifo_full_int_route |= INT_ROUTE_PIN_2;
    }

    int_routes->data_int_route = INT_ROUTE_NONE;
    if ((data[1] & 0x01) != 0) {
        int_routes->data_int_route |= INT_ROUTE_PIN_1;
    }
    if ((data[1] & 0x80) != 0) {
        int_routes->data_int_route |= INT_ROUTE_PIN_2;
    }

    return 0;
}

int
bma253_set_int_routes(const struct bma253 * bma253,
                      const struct int_routes * int_routes)
{
    uint8_t data[3];
    int rc;

    data[0] = (((int_routes->flat_int_route & INT_ROUTE_PIN_1) != 0) << 7) |
              (((int_routes->orient_int_route & INT_ROUTE_PIN_1) != 0) << 6) |
              (((int_routes->s_tap_int_route & INT_ROUTE_PIN_1) != 0) << 5) |
              (((int_routes->d_tap_int_route & INT_ROUTE_PIN_1) != 0) << 4) |
              (((int_routes->slow_no_mot_int_route & INT_ROUTE_PIN_1) != 0) << 3) |
              (((int_routes->slope_int_route & INT_ROUTE_PIN_1) != 0) << 2) |
              (((int_routes->high_g_int_route & INT_ROUTE_PIN_1) != 0) << 1) |
              (((int_routes->low_g_int_route & INT_ROUTE_PIN_1) != 0) << 0);

    data[1] = (((int_routes->data_int_route & INT_ROUTE_PIN_2) != 0) << 7) |
              (((int_routes->fifo_wmark_int_route & INT_ROUTE_PIN_2) != 0) << 6) |
              (((int_routes->fifo_full_int_route & INT_ROUTE_PIN_2) != 0) << 5) |
              (((int_routes->fifo_full_int_route & INT_ROUTE_PIN_1) != 0) << 2) |
              (((int_routes->fifo_wmark_int_route & INT_ROUTE_PIN_1) != 0) << 1) |
              (((int_routes->data_int_route & INT_ROUTE_PIN_1) != 0) << 0);

    data[2] = (((int_routes->flat_int_route & INT_ROUTE_PIN_2) != 0) << 7) |
              (((int_routes->orient_int_route & INT_ROUTE_PIN_2) != 0) << 6) |
              (((int_routes->s_tap_int_route & INT_ROUTE_PIN_2) != 0) << 5) |
              (((int_routes->d_tap_int_route & INT_ROUTE_PIN_2) != 0) << 4) |
              (((int_routes->slow_no_mot_int_route & INT_ROUTE_PIN_2) != 0) << 3) |
              (((int_routes->slope_int_route & INT_ROUTE_PIN_2) != 0) << 2) |
              (((int_routes->high_g_int_route & INT_ROUTE_PIN_2) != 0) << 1) |
              (((int_routes->low_g_int_route & INT_ROUTE_PIN_2) != 0) << 0);

    rc = set_register(bma253, REG_ADDR_INT_MAP_0, data[0]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_MAP_1, data[1]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_MAP_2, data[2]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_int_filters(const struct bma253 * bma253,
                       struct int_filters * int_filters)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_INT_SRC, &data);
    if (rc != 0) {
        return rc;
    }

    int_filters->unfiltered_data_int = data & 0x20;
    int_filters->unfiltered_tap_int = data & 0x10;
    int_filters->unfiltered_slow_no_mot_int = data & 0x08;
    int_filters->unfiltered_slope_int = data & 0x04;
    int_filters->unfiltered_high_g_int = data & 0x02;
    int_filters->unfiltered_low_g_int = data & 0x01;

    return 0;
}

int
bma253_set_int_filters(const struct bma253 * bma253,
                       const struct int_filters * int_filters)
{
    uint8_t data;

    data = (int_filters->unfiltered_data_int << 5) |
           (int_filters->unfiltered_tap_int << 4) |
           (int_filters->unfiltered_slow_no_mot_int << 3) |
           (int_filters->unfiltered_slope_int << 2) |
           (int_filters->unfiltered_high_g_int << 1) |
           (int_filters->unfiltered_low_g_int << 0);

    return set_register(bma253, REG_ADDR_INT_SRC, data);
}

int
bma253_get_int_pin_electrical(const struct bma253 * bma253,
                              struct int_pin_electrical * electrical)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_INT_OUT_CTRL, &data);
    if (rc != 0) {
        return rc;
    }

    if ((data & 0x02) != 0) {
        electrical->pin1_output = INT_PIN_OUTPUT_OPEN_DRAIN;
    } else {
        electrical->pin1_output = INT_PIN_OUTPUT_PUSH_PULL;
    }
    if ((data & 0x01) != 0) {
        electrical->pin1_active = INT_PIN_ACTIVE_HIGH;
    } else {
        electrical->pin1_active = INT_PIN_ACTIVE_LOW;
    }
    if ((data & 0x08) != 0) {
        electrical->pin2_output = INT_PIN_OUTPUT_OPEN_DRAIN;
    } else {
        electrical->pin2_output = INT_PIN_OUTPUT_PUSH_PULL;
    }
    if ((data & 0x04) != 0) {
        electrical->pin2_active = INT_PIN_ACTIVE_HIGH;
    } else {
        electrical->pin2_active = INT_PIN_ACTIVE_LOW;
    }

    return 0;
}

int
bma253_set_int_pin_electrical(const struct bma253 * bma253,
                              const struct int_pin_electrical * electrical)
{
    uint8_t data;

    data = 0;

    switch (electrical->pin1_output) {
    case INT_PIN_OUTPUT_OPEN_DRAIN:
        data |= 0x02;
        break;
    case INT_PIN_OUTPUT_PUSH_PULL:
        data |= 0x00;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (electrical->pin1_active) {
    case INT_PIN_ACTIVE_HIGH:
        data |= 0x01;
        break;
    case INT_PIN_ACTIVE_LOW:
        data |= 0x00;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (electrical->pin2_output) {
    case INT_PIN_OUTPUT_OPEN_DRAIN:
        data |= 0x08;
        break;
    case INT_PIN_OUTPUT_PUSH_PULL:
        data |= 0x00;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (electrical->pin2_active) {
    case INT_PIN_ACTIVE_HIGH:
        data |= 0x04;
        break;
    case INT_PIN_ACTIVE_LOW:
        data |= 0x00;
        break;
    default:
        return SYS_EINVAL;
    }

    return set_register(bma253, REG_ADDR_INT_OUT_CTRL, data);
}

int
bma253_get_int_latch(const struct bma253 * bma253,
                     enum int_latch * int_latch)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_INT_RST_LATCH, &data);
    if (rc != 0) {
        return rc;
    }

    switch (data & 0x0F) {
    case 0x00:
        *int_latch = INT_LATCH_NON_LATCHED;
        break;
    case 0x01:
        *int_latch = INT_LATCH_TEMPORARY_250_MS;
        break;
    case 0x02:
        *int_latch = INT_LATCH_TEMPORARY_500_MS;
        break;
    case 0x03:
        *int_latch = INT_LATCH_TEMPORARY_1_S;
        break;
    case 0x04:
        *int_latch = INT_LATCH_TEMPORARY_2_S;
        break;
    case 0x05:
        *int_latch = INT_LATCH_TEMPORARY_4_S;
        break;
    case 0x06:
        *int_latch = INT_LATCH_TEMPORARY_8_S;
        break;
    case 0x07:
        *int_latch = INT_LATCH_LATCHED;
        break;
    case 0x08:
        *int_latch = INT_LATCH_NON_LATCHED;
        break;
    case 0x09:
        *int_latch = INT_LATCH_TEMPORARY_250_US;
        break;
    case 0x0A:
        *int_latch = INT_LATCH_TEMPORARY_500_US;
        break;
    case 0x0B:
        *int_latch = INT_LATCH_TEMPORARY_1_MS;
        break;
    case 0x0C:
        *int_latch = INT_LATCH_TEMPORARY_12_5_MS;
        break;
    case 0x0D:
        *int_latch = INT_LATCH_TEMPORARY_25_MS;
        break;
    case 0x0E:
        *int_latch = INT_LATCH_TEMPORARY_50_MS;
        break;
    case 0x0F:
        *int_latch = INT_LATCH_LATCHED;
        break;
    }

    return 0;
}

int
bma253_set_int_latch(const struct bma253 * bma253,
                     bool reset_ints,
                     enum int_latch int_latch)
{
    uint8_t data;

    data = 0;
    data |= reset_ints << 7;

    switch (int_latch) {
    case INT_LATCH_NON_LATCHED:
        data |= 0x00;
        break;
    case INT_LATCH_LATCHED:
        data |= 0x0F;
        break;
    case INT_LATCH_TEMPORARY_250_US:
        data |= 0x09;
        break;
    case INT_LATCH_TEMPORARY_500_US:
        data |= 0x0A;
        break;
    case INT_LATCH_TEMPORARY_1_MS:
        data |= 0x0B;
        break;
    case INT_LATCH_TEMPORARY_12_5_MS:
        data |= 0x0C;
        break;
    case INT_LATCH_TEMPORARY_25_MS:
        data |= 0x0D;
        break;
    case INT_LATCH_TEMPORARY_50_MS:
        data |= 0x0E;
        break;
    case INT_LATCH_TEMPORARY_250_MS:
        data |= 0x01;
        break;
    case INT_LATCH_TEMPORARY_500_MS:
        data |= 0x02;
        break;
    case INT_LATCH_TEMPORARY_1_S:
        data |= 0x03;
        break;
    case INT_LATCH_TEMPORARY_2_S:
        data |= 0x04;
        break;
    case INT_LATCH_TEMPORARY_4_S:
        data |= 0x05;
        break;
    case INT_LATCH_TEMPORARY_8_S:
        data |= 0x06;
        break;
    default:
        return SYS_EINVAL;
    }

    return set_register(bma253, REG_ADDR_INT_RST_LATCH, data);
}

int
bma253_get_low_g_int_cfg(const struct bma253 * bma253,
                         struct low_g_int_cfg * low_g_int_cfg)
{
    uint8_t data[3];
    int rc;

    rc = get_registers(bma253, REG_ADDR_INT_0,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    low_g_int_cfg->delay_ms = (data[0] + 1) << 1;
    low_g_int_cfg->thresh_g = (float)data[1] * 0.00781;
    low_g_int_cfg->hyster_g = (float)(data[2] & 0x03) * 0.125;
    low_g_int_cfg->axis_summing = data[2] & 0x04;

    return 0;
}

int
bma253_set_low_g_int_cfg(const struct bma253 * bma253,
                         const struct low_g_int_cfg * low_g_int_cfg)
{
    uint8_t data[3];
    int rc;

    if (low_g_int_cfg->delay_ms < 2 ||
        low_g_int_cfg->delay_ms > 512) {
        return SYS_EINVAL;
    }
    if (low_g_int_cfg->thresh_g < 0.0 ||
        low_g_int_cfg->thresh_g > 1.992) {
        return SYS_EINVAL;
    }
    if (low_g_int_cfg->hyster_g < 0.0 ||
        low_g_int_cfg->hyster_g > 0.375) {
        return SYS_EINVAL;
    }

    data[0] = (low_g_int_cfg->delay_ms >> 1) - 1;
    data[1] = low_g_int_cfg->thresh_g / 0.00781;
    data[2] = (low_g_int_cfg->axis_summing << 2) |
              (((uint8_t)(low_g_int_cfg->hyster_g / 0.125) & 0x03) << 0);

    rc = set_register(bma253, REG_ADDR_INT_0, data[0]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_1, data[1]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_2, data[2]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_high_g_int_cfg(const struct bma253 * bma253,
                          enum bma253_g_range g_range,
                          struct high_g_int_cfg * high_g_int_cfg)
{
    float hyster_scale;
    float thresh_scale;
    uint8_t data[3];
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        hyster_scale = 0.125;
        thresh_scale = 0.00781;
        break;
    case BMA253_G_RANGE_4:
        hyster_scale = 0.25;
        thresh_scale = 0.01563;
        break;
    case BMA253_G_RANGE_8:
        hyster_scale = 0.5;
        thresh_scale = 0.03125;
        break;
    case BMA253_G_RANGE_16:
        hyster_scale = 1.0;
        thresh_scale = 0.0625;
        break;
    default:
        return SYS_EINVAL;
    }

    rc = get_registers(bma253, REG_ADDR_INT_2,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    high_g_int_cfg->hyster_g = (float)((data[0] >> 6) & 0x03) * hyster_scale;
    high_g_int_cfg->delay_ms = (data[1] + 1) << 1;
    high_g_int_cfg->thresh_g = (float)data[2] * thresh_scale;

    return 0;
}

int
bma253_set_high_g_int_cfg(const struct bma253 * bma253,
                          enum bma253_g_range g_range,
                          const struct high_g_int_cfg * high_g_int_cfg)
{
    float hyster_scale;
    float thresh_scale;
    uint8_t data[3];
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        hyster_scale = 0.125;
        thresh_scale = 0.00781;
        break;
    case BMA253_G_RANGE_4:
        hyster_scale = 0.25;
        thresh_scale = 0.01563;
        break;
    case BMA253_G_RANGE_8:
        hyster_scale = 0.5;
        thresh_scale = 0.03125;
        break;
    case BMA253_G_RANGE_16:
        hyster_scale = 1.0;
        thresh_scale = 0.0625;
        break;
    default:
        return SYS_EINVAL;
    }

    if (high_g_int_cfg->hyster_g < 0.0 ||
        high_g_int_cfg->hyster_g > hyster_scale * 3.0) {
        return SYS_EINVAL;
    }
    if (high_g_int_cfg->delay_ms < 2 ||
        high_g_int_cfg->delay_ms > 512) {
        return SYS_EINVAL;
    }
    if (high_g_int_cfg->thresh_g < 0.0 ||
        high_g_int_cfg->thresh_g > thresh_scale * 255.0) {
        return SYS_EINVAL;
    }

    data[0] = ((uint8_t)(high_g_int_cfg->hyster_g / hyster_scale) & 0x03) << 6;
    data[1] = (high_g_int_cfg->delay_ms >> 1) - 1;
    data[2] = high_g_int_cfg->thresh_g / thresh_scale;

    rc = set_register(bma253, REG_ADDR_INT_2, data[0]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_3, data[1]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_4, data[2]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_slow_no_mot_int_cfg(const struct bma253 * bma253,
                               bool no_motion_select,
                               enum bma253_g_range g_range,
                               struct slow_no_mot_int_cfg * slow_no_mot_int_cfg)
{
    float thresh_scale;
    uint8_t data[2];
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        thresh_scale = 0.00391;
        break;
    case BMA253_G_RANGE_4:
        thresh_scale = 0.00781;
        break;
    case BMA253_G_RANGE_8:
        thresh_scale = 0.01563;
        break;
    case BMA253_G_RANGE_16:
        thresh_scale = 0.03125;
        break;
    default:
        return SYS_EINVAL;
    }

    rc = get_register(bma253, REG_ADDR_INT_5, data + 0);
    if (rc != 0) {
        return rc;
    }
    rc = get_register(bma253, REG_ADDR_INT_7, data + 1);
    if (rc != 0) {
        return rc;
    }

    if (no_motion_select) {
        if ((data[0] & 0x80) == 0) {
            if ((data[0] & 0x40) == 0) {
                slow_no_mot_int_cfg->duration_p_or_s =
                    ((data[0] >> 2) & 0x0F) + 1;
            } else {
                slow_no_mot_int_cfg->duration_p_or_s =
                    (((data[0] >> 2) & 0x0F) << 2) + 20;
            }
        } else {
            slow_no_mot_int_cfg->duration_p_or_s =
                (((data[0] >> 2) & 0x1F) << 3) + 88;
        }
    } else {
        slow_no_mot_int_cfg->duration_p_or_s =
            ((data[0] >> 2) & 0x03) + 1;
    }
    slow_no_mot_int_cfg->thresh_g = (float)data[1] * thresh_scale;

    return 0;
}

int
bma253_set_slow_no_mot_int_cfg(const struct bma253 * bma253,
                               bool no_motion_select,
                               enum bma253_g_range g_range,
                               const struct slow_no_mot_int_cfg * slow_no_mot_int_cfg)
{
    float thresh_scale;
    uint8_t data[2];
    uint16_t duration;
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        thresh_scale = 0.00391;
        break;
    case BMA253_G_RANGE_4:
        thresh_scale = 0.00781;
        break;
    case BMA253_G_RANGE_8:
        thresh_scale = 0.01563;
        break;
    case BMA253_G_RANGE_16:
        thresh_scale = 0.03125;
        break;
    default:
        return SYS_EINVAL;
    }

    if (no_motion_select) {
        if (slow_no_mot_int_cfg->duration_p_or_s < 1 ||
            slow_no_mot_int_cfg->duration_p_or_s > 336) {
            return SYS_EINVAL;
        }
    } else {
        if (slow_no_mot_int_cfg->duration_p_or_s < 1 ||
            slow_no_mot_int_cfg->duration_p_or_s > 4) {
            return SYS_EINVAL;
        }
    }
    if (slow_no_mot_int_cfg->thresh_g < 0.0 ||
        slow_no_mot_int_cfg->thresh_g > thresh_scale * 255.0) {
        return SYS_EINVAL;
    }

    duration = slow_no_mot_int_cfg->duration_p_or_s;
    if (no_motion_select) {
        if (duration > 80) {
            if (duration < 88) {
                duration = 88;
            }
            data[0] = (((duration - 88) >> 3) << 2) | 0x80;
        } else if (duration > 16) {
            if (duration < 20) {
                duration = 20;
            }
            data[0] = (((duration - 20) >> 2) << 2) | 0x40;
        } else {
            data[0] = (duration - 1) << 2;
        }
    } else {
        data[0] = (duration - 1) << 2;
    }
    data[1] = slow_no_mot_int_cfg->thresh_g / thresh_scale;

    rc = set_register(bma253, REG_ADDR_INT_5, data[0]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_7, data[1]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_slope_int_cfg(const struct bma253 * bma253,
                         enum bma253_g_range g_range,
                         struct slope_int_cfg * slope_int_cfg)
{
    float thresh_scale;
    uint8_t data[2];
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        thresh_scale = 0.00391;
        break;
    case BMA253_G_RANGE_4:
        thresh_scale = 0.00781;
        break;
    case BMA253_G_RANGE_8:
        thresh_scale = 0.01563;
        break;
    case BMA253_G_RANGE_16:
        thresh_scale = 0.03125;
        break;
    default:
        return SYS_EINVAL;
    }

    rc = get_registers(bma253, REG_ADDR_INT_5,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    slope_int_cfg->duration_p = (data[0] & 0x03) + 1;
    slope_int_cfg->thresh_g = (float)data[1] * thresh_scale;

    return 0;
}

int
bma253_set_slope_int_cfg(const struct bma253 * bma253,
                         enum bma253_g_range g_range,
                         const struct slope_int_cfg * slope_int_cfg)
{
    float thresh_scale;
    uint8_t data[2];
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        thresh_scale = 0.00391;
        break;
    case BMA253_G_RANGE_4:
        thresh_scale = 0.00781;
        break;
    case BMA253_G_RANGE_8:
        thresh_scale = 0.01563;
        break;
    case BMA253_G_RANGE_16:
        thresh_scale = 0.03125;
        break;
    default:
        return SYS_EINVAL;
    }

    if (slope_int_cfg->duration_p < 1 ||
        slope_int_cfg->duration_p > 4) {
        return SYS_EINVAL;
    }
    if (slope_int_cfg->thresh_g < 0.0 ||
        slope_int_cfg->thresh_g > thresh_scale * 255.0) {
        return SYS_EINVAL;
    }

    data[0] = (slope_int_cfg->duration_p - 1) & 0x03;
    data[1] = slope_int_cfg->thresh_g / thresh_scale;

    rc = set_register(bma253, REG_ADDR_INT_5, data[0]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_6, data[1]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_tap_int_cfg(const struct bma253 * bma253,
                       enum bma253_g_range g_range,
                       struct tap_int_cfg * tap_int_cfg)
{
    float thresh_scale;
    uint8_t data[2];
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        thresh_scale = 0.0625;
        break;
    case BMA253_G_RANGE_4:
        thresh_scale = 0.125;
        break;
    case BMA253_G_RANGE_8:
        thresh_scale = 0.25;
        break;
    case BMA253_G_RANGE_16:
        thresh_scale = 0.5;
        break;
    default:
        return SYS_EINVAL;
    }

    rc = get_registers(bma253, REG_ADDR_INT_8,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    if ((data[0] & 0x80) == 0) {
        tap_int_cfg->tap_quiet = BMA253_TAP_QUIET_30_MS;
    } else {
        tap_int_cfg->tap_quiet = BMA253_TAP_QUIET_20_MS;
    }
    if ((data[0] & 0x40) == 0) {
        tap_int_cfg->tap_shock = BMA253_TAP_SHOCK_50_MS;
    } else {
        tap_int_cfg->tap_shock = BMA253_TAP_SHOCK_75_MS;
    }

    switch (data[0] & 0x07) {
    case 0x00:
        tap_int_cfg->d_tap_window = BMA253_D_TAP_WINDOW_50_MS;
        break;
    case 0x01:
        tap_int_cfg->d_tap_window = BMA253_D_TAP_WINDOW_100_MS;
        break;
    case 0x02:
        tap_int_cfg->d_tap_window = BMA253_D_TAP_WINDOW_150_MS;
        break;
    case 0x03:
        tap_int_cfg->d_tap_window = BMA253_D_TAP_WINDOW_200_MS;
        break;
    case 0x04:
        tap_int_cfg->d_tap_window = BMA253_D_TAP_WINDOW_250_MS;
        break;
    case 0x05:
        tap_int_cfg->d_tap_window = BMA253_D_TAP_WINDOW_375_MS;
        break;
    case 0x06:
        tap_int_cfg->d_tap_window = BMA253_D_TAP_WINDOW_500_MS;
        break;
    case 0x07:
        tap_int_cfg->d_tap_window = BMA253_D_TAP_WINDOW_700_MS;
        break;
    }

    switch ((data[1] >> 6) & 0x03) {
    case 0x00:
        tap_int_cfg->tap_wake_samples = BMA253_TAP_WAKE_SAMPLES_2;
        break;
    case 0x01:
        tap_int_cfg->tap_wake_samples = BMA253_TAP_WAKE_SAMPLES_4;
        break;
    case 0x02:
        tap_int_cfg->tap_wake_samples = BMA253_TAP_WAKE_SAMPLES_8;
        break;
    case 0x03:
        tap_int_cfg->tap_wake_samples = BMA253_TAP_WAKE_SAMPLES_16;
        break;
    }

    tap_int_cfg->thresh_g = (float)(data[1] & 0x1F) * thresh_scale;

    return 0;
}

int
bma253_set_tap_int_cfg(const struct bma253 * bma253,
                       enum bma253_g_range g_range,
                       const struct tap_int_cfg * tap_int_cfg)
{
    float thresh_scale;
    uint8_t data[2];
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        thresh_scale = 0.0625;
        break;
    case BMA253_G_RANGE_4:
        thresh_scale = 0.125;
        break;
    case BMA253_G_RANGE_8:
        thresh_scale = 0.25;
        break;
    case BMA253_G_RANGE_16:
        thresh_scale = 0.5;
        break;
    default:
        return SYS_EINVAL;
    }

    if (tap_int_cfg->thresh_g < 0.0 ||
        tap_int_cfg->thresh_g > thresh_scale * 31.0) {
        return SYS_EINVAL;
    }

    data[0] = 0;
    data[1] = 0;

    switch (tap_int_cfg->tap_quiet) {
    case BMA253_TAP_QUIET_20_MS:
        data[0] |= 0x80;
        break;
    case BMA253_TAP_QUIET_30_MS:
        data[0] |= 0x00;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (tap_int_cfg->tap_shock) {
    case BMA253_TAP_SHOCK_50_MS:
        data[0] |= 0x00;
        break;
    case BMA253_TAP_SHOCK_75_MS:
        data[0] |= 0x40;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (tap_int_cfg->d_tap_window) {
    case BMA253_D_TAP_WINDOW_50_MS:
        data[0] |= 0x00;
        break;
    case BMA253_D_TAP_WINDOW_100_MS:
        data[0] |= 0x01;
        break;
    case BMA253_D_TAP_WINDOW_150_MS:
        data[0] |= 0x02;
        break;
    case BMA253_D_TAP_WINDOW_200_MS:
        data[0] |= 0x03;
        break;
    case BMA253_D_TAP_WINDOW_250_MS:
        data[0] |= 0x04;
        break;
    case BMA253_D_TAP_WINDOW_375_MS:
        data[0] |= 0x05;
        break;
    case BMA253_D_TAP_WINDOW_500_MS:
        data[0] |= 0x06;
        break;
    case BMA253_D_TAP_WINDOW_700_MS:
        data[0] |= 0x07;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (tap_int_cfg->tap_wake_samples) {
    case BMA253_TAP_WAKE_SAMPLES_2:
        data[1] |= 0x00 << 6;
        break;
    case BMA253_TAP_WAKE_SAMPLES_4:
        data[1] |= 0x01 << 6;
        break;
    case BMA253_TAP_WAKE_SAMPLES_8:
        data[1] |= 0x02 << 6;
        break;
    case BMA253_TAP_WAKE_SAMPLES_16:
        data[1] |= 0x03 << 6;
        break;
    default:
        return SYS_EINVAL;
    }

    data[1] |= (uint8_t)(tap_int_cfg->thresh_g / thresh_scale) & 0x1F;

    rc = set_register(bma253, REG_ADDR_INT_8, data[0]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_9, data[1]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_orient_int_cfg(const struct bma253 * bma253,
                          struct orient_int_cfg * orient_int_cfg)
{
    uint8_t data[2];
    int rc;

    rc = get_registers(bma253, REG_ADDR_INT_A,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    orient_int_cfg->hyster_g = (float)((data[0] >> 4) & 0x07) * 0.0625;

    switch ((data[0] >> 2) & 0x03) {
    case 0x00:
        orient_int_cfg->orient_blocking =
            BMA253_ORIENT_BLOCKING_NONE;
        break;
    case 0x01:
        orient_int_cfg->orient_blocking =
            BMA253_ORIENT_BLOCKING_ACCEL_ONLY;
        break;
    case 0x02:
        orient_int_cfg->orient_blocking =
            BMA253_ORIENT_BLOCKING_ACCEL_AND_SLOPE;
        break;
    case 0x03:
        orient_int_cfg->orient_blocking =
            BMA253_ORIENT_BLOCKING_ACCEL_AND_SLOPE_AND_STABLE;
        break;
    }

    switch (data[0] & 0x03) {
    case 0x00:
        orient_int_cfg->orient_mode =
            BMA253_ORIENT_MODE_SYMMETRICAL;
        break;
    case 0x01:
        orient_int_cfg->orient_mode =
            BMA253_ORIENT_MODE_HIGH_ASYMMETRICAL;
        break;
    case 0x02:
        orient_int_cfg->orient_mode =
            BMA253_ORIENT_MODE_LOW_ASYMMETRICAL;
        break;
    case 0x03:
        orient_int_cfg->orient_mode =
            BMA253_ORIENT_MODE_SYMMETRICAL;
        break;
    }

    orient_int_cfg->signal_up_dn = data[1] & 0x40;
    orient_int_cfg->blocking_angle = data[1] & 0x3F;

    return 0;
}

int
bma253_set_orient_int_cfg(const struct bma253 * bma253,
                          const struct orient_int_cfg * orient_int_cfg)
{
    uint8_t data[2];
    int rc;

    if (orient_int_cfg->hyster_g < 0.0 ||
        orient_int_cfg->hyster_g > (0.0625 * 7.0)) {
        return SYS_EINVAL;
    }
    if (orient_int_cfg->blocking_angle > 0x3F) {
        return SYS_EINVAL;
    }

    data[0] = (uint8_t)(orient_int_cfg->hyster_g / 0.0625) << 4;

    switch (orient_int_cfg->orient_blocking) {
    case BMA253_ORIENT_BLOCKING_NONE:
        data[0] |= 0x00 << 2;
        break;
    case BMA253_ORIENT_BLOCKING_ACCEL_ONLY:
        data[0] |= 0x01 << 2;
        break;
    case BMA253_ORIENT_BLOCKING_ACCEL_AND_SLOPE:
        data[0] |= 0x02 << 2;
        break;
    case BMA253_ORIENT_BLOCKING_ACCEL_AND_SLOPE_AND_STABLE:
        data[0] |= 0x03 << 2;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (orient_int_cfg->orient_mode) {
    case BMA253_ORIENT_MODE_SYMMETRICAL:
        data[0] |= 0x00;
        break;
    case BMA253_ORIENT_MODE_HIGH_ASYMMETRICAL:
        data[0] |= 0x01;
        break;
    case BMA253_ORIENT_MODE_LOW_ASYMMETRICAL:
        data[0] |= 0x02;
        break;
    default:
        return SYS_EINVAL;
    }

    data[1] = (orient_int_cfg->signal_up_dn << 6) |
              ((orient_int_cfg->blocking_angle & 0x3F) << 0);

    rc = set_register(bma253, REG_ADDR_INT_A, data[0]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_B, data[1]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_flat_int_cfg(const struct bma253 * bma253,
                        struct flat_int_cfg * flat_int_cfg)
{
    uint8_t data[2];
    int rc;

    rc = get_registers(bma253, REG_ADDR_INT_C,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    flat_int_cfg->flat_angle = data[0] & 0x3F;

    switch ((data[1] >> 4) & 0x03) {
    case 0x00:
        flat_int_cfg->flat_hold = FLAT_HOLD_0_MS;
        break;
    case 0x01:
        flat_int_cfg->flat_hold = FLAT_HOLD_512_MS;
        break;
    case 0x02:
        flat_int_cfg->flat_hold = FLAT_HOLD_1024_MS;
        break;
    case 0x03:
        flat_int_cfg->flat_hold = FLAT_HOLD_2048_MS;
        break;
    }

    flat_int_cfg->flat_hyster = data[1] & 0x07;
    flat_int_cfg->hyster_enable = (data[1] & 0x07) != 0x00;

    return 0;
}

int
bma253_set_flat_int_cfg(const struct bma253 * bma253,
                        const struct flat_int_cfg * flat_int_cfg)
{
    uint8_t data[2];
    int rc;

    if (flat_int_cfg->flat_angle > 0x3F) {
        return SYS_EINVAL;
    }
    if (flat_int_cfg->flat_hyster == 0x00 &&
        flat_int_cfg->hyster_enable) {
        return SYS_EINVAL;
    }

    data[0] = flat_int_cfg->flat_angle & 0x3F;
    data[1] = 0;

    switch (flat_int_cfg->flat_hold) {
    case FLAT_HOLD_0_MS:
        data[1] |= 0x00 << 4;
        break;
    case FLAT_HOLD_512_MS:
        data[1] |= 0x01 << 4;
        break;
    case FLAT_HOLD_1024_MS:
        data[1] |= 0x02 << 4;
        break;
    case FLAT_HOLD_2048_MS:
        data[1] |= 0x03 << 4;
        break;
    }

    if (flat_int_cfg->hyster_enable) {
        data[1] |= flat_int_cfg->flat_hyster & 0x07;
    }

    rc = set_register(bma253, REG_ADDR_INT_C, data[0]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_INT_D, data[1]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_fifo_wmark_level(const struct bma253 * bma253,
                            uint8_t * wmark_level)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_FIFO_CONFIG_0, &data);
    if (rc != 0) {
        return rc;
    }

    *wmark_level = data & 0x3F;

    return 0;
}

int
bma253_set_fifo_wmark_level(const struct bma253 * bma253,
                            uint8_t wmark_level)
{
    uint8_t data;

    if (wmark_level > 32) {
        return SYS_EINVAL;
    }

    data = wmark_level & 0x3F;

    return set_register(bma253, REG_ADDR_FIFO_CONFIG_0, data);
}

int
bma253_get_self_test_cfg(const struct bma253 * bma253,
                         struct self_test_cfg * self_test_cfg)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_PMU_SELF_TEST, &data);
    if (rc != 0) {
        return rc;
    }

    if ((data & 0x10) == 0) {
        self_test_cfg->self_test_ampl = SELF_TEST_AMPL_LOW;
    } else {
        self_test_cfg->self_test_ampl = SELF_TEST_AMPL_HIGH;
    }
    if ((data & 0x04) == 0) {
        self_test_cfg->self_test_sign = SELF_TEST_SIGN_NEGATIVE;
    } else {
        self_test_cfg->self_test_sign = SELF_TEST_SIGN_POSITIVE;
    }

    switch (data & 0x03) {
    case 0x00:
        self_test_cfg->self_test_axis = -1;
        self_test_cfg->self_test_enabled = false;
        break;
    case 0x01:
        self_test_cfg->self_test_axis = AXIS_X;
        self_test_cfg->self_test_enabled = true;
        break;
    case 0x02:
        self_test_cfg->self_test_axis = AXIS_Y;
        self_test_cfg->self_test_enabled = true;
        break;
    case 0x03:
        self_test_cfg->self_test_axis = AXIS_Z;
        self_test_cfg->self_test_enabled = true;
        break;
    }

    return 0;
}

int
bma253_set_self_test_cfg(const struct bma253 * bma253,
                         const struct self_test_cfg * self_test_cfg)
{
    uint8_t data;

    data = 0;

    switch (self_test_cfg->self_test_ampl) {
    case SELF_TEST_AMPL_HIGH:
        data |= 0x10;
        break;
    case SELF_TEST_AMPL_LOW:
        data |= 0x00;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (self_test_cfg->self_test_sign) {
    case SELF_TEST_SIGN_NEGATIVE:
        data |= 0x00;
        break;
    case SELF_TEST_SIGN_POSITIVE:
        data |= 0x04;
        break;
    default:
        return SYS_EINVAL;
    }

    if (self_test_cfg->self_test_enabled) {
        switch (self_test_cfg->self_test_axis) {
        case AXIS_X:
            data |= 0x01;
            break;
        case AXIS_Y:
            data |= 0x02;
            break;
        case AXIS_Z:
            data |= 0x03;
            break;
        default:
            return SYS_EINVAL;
        }
    }

    return set_register(bma253, REG_ADDR_PMU_SELF_TEST, data);
}

int
bma253_get_nvm_control(const struct bma253 * bma253,
                       uint8_t * remaining_cycles,
                       bool * load_from_nvm,
                       bool * nvm_is_ready,
                       bool * nvm_unlocked)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_TRIM_NVM_CTRL, &data);
    if (rc != 0) {
        return rc;
    }

    *remaining_cycles = (data >> 4) & 0x0F;
    *load_from_nvm = data & 0x08;
    *nvm_is_ready = data & 0x04;
    *nvm_unlocked = data & 0x01;

    return 0;
}

int
bma253_set_nvm_control(const struct bma253 * bma253,
                       bool load_from_nvm,
                       bool store_into_nvm,
                       bool nvm_unlocked)
{
    uint8_t data;

    data = (load_from_nvm << 3) |
           (store_into_nvm << 1) |
           (nvm_unlocked << 0);

    return set_register(bma253, REG_ADDR_TRIM_NVM_CTRL, data);
}

int
bma253_get_i2c_watchdog(const struct bma253 * bma253,
                        enum i2c_watchdog * i2c_watchdog)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_BGW_SPI3_WDT, &data);
    if (rc != 0) {
        return rc;
    }

    if ((data & 0x04) != 0) {
        if ((data & 0x02) != 0) {
            *i2c_watchdog = I2C_WATCHDOG_50_MS;
        } else {
            *i2c_watchdog = I2C_WATCHDOG_1_MS;
        }
    } else {
        *i2c_watchdog = I2C_WATCHDOG_DISABLED;
    }

    return 0;
}

int
bma253_set_i2c_watchdog(const struct bma253 * bma253,
                        enum i2c_watchdog i2c_watchdog)
{
    uint8_t data;

    switch (i2c_watchdog) {
    case I2C_WATCHDOG_DISABLED:
        data = 0x00;
        break;
    case I2C_WATCHDOG_1_MS:
        data = 0x04;
        break;
    case I2C_WATCHDOG_50_MS:
        data = 0x06;
        break;
    default:
        return SYS_EINVAL;
    }

    return set_register(bma253, REG_ADDR_BGW_SPI3_WDT, data);
}

int
bma253_get_fast_ofc_cfg(const struct bma253 * bma253,
                        bool * fast_ofc_ready,
                        enum bma253_offset_comp_target * ofc_target_z,
                        enum bma253_offset_comp_target * ofc_target_y,
                        enum bma253_offset_comp_target * ofc_target_x)
{
    uint8_t data[2];
    int rc;

    rc = get_registers(bma253, REG_ADDR_OFC_CTRL,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    *fast_ofc_ready = data[0] & 0x10;

    switch ((data[1] >> 5) & 0x03) {
    case 0x00:
        *ofc_target_z = BMA253_OFFSET_COMP_TARGET_0_G;
        break;
    case 0x01:
        *ofc_target_z = BMA253_OFFSET_COMP_TARGET_POS_1_G;
        break;
    case 0x02:
        *ofc_target_z = BMA253_OFFSET_COMP_TARGET_NEG_1_G;
        break;
    case 0x03:
        *ofc_target_z = BMA253_OFFSET_COMP_TARGET_0_G;
        break;
    }

    switch ((data[1] >> 3) & 0x03) {
    case 0x00:
        *ofc_target_y = BMA253_OFFSET_COMP_TARGET_0_G;
        break;
    case 0x01:
        *ofc_target_y = BMA253_OFFSET_COMP_TARGET_POS_1_G;
        break;
    case 0x02:
        *ofc_target_y = BMA253_OFFSET_COMP_TARGET_NEG_1_G;
        break;
    case 0x03:
        *ofc_target_y = BMA253_OFFSET_COMP_TARGET_0_G;
        break;
    }

    switch ((data[1] >> 1) & 0x03) {
    case 0x00:
        *ofc_target_x = BMA253_OFFSET_COMP_TARGET_0_G;
        break;
    case 0x01:
        *ofc_target_x = BMA253_OFFSET_COMP_TARGET_POS_1_G;
        break;
    case 0x02:
        *ofc_target_x = BMA253_OFFSET_COMP_TARGET_NEG_1_G;
        break;
    case 0x03:
        *ofc_target_x = BMA253_OFFSET_COMP_TARGET_0_G;
        break;
    }

    return 0;
}

int
bma253_set_fast_ofc_cfg(const struct bma253 * bma253,
                        enum axis fast_ofc_axis,
                        enum bma253_offset_comp_target fast_ofc_target,
                        bool trigger_fast_ofc)
{
    uint8_t data[2];
    uint8_t axis_value;
    uint8_t axis_shift;
    int rc;

    data[0] = 0;
    data[1] = 0;

    switch (fast_ofc_axis) {
    case AXIS_X:
        axis_value = 0x01;
        axis_shift = 1;
        break;
    case AXIS_Y:
        axis_value = 0x02;
        axis_shift = 3;
        break;
    case AXIS_Z:
        axis_value = 0x03;
        axis_shift = 5;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (fast_ofc_target) {
    case BMA253_OFFSET_COMP_TARGET_0_G:
        data[1] |= 0x00 << axis_shift;
        break;
    case BMA253_OFFSET_COMP_TARGET_NEG_1_G:
        data[1] |= 0x02 << axis_shift;
        break;
    case BMA253_OFFSET_COMP_TARGET_POS_1_G:
        data[1] |= 0x01 << axis_shift;
        break;
    default:
        return SYS_EINVAL;
    }

    if (trigger_fast_ofc) {
        data[0] |= axis_value << 5;
    }

    rc = set_register(bma253, REG_ADDR_OFC_SETTING, data[1]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_OFC_CTRL, data[0]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_get_slow_ofc_cfg(const struct bma253 * bma253,
                        struct slow_ofc_cfg * slow_ofc_cfg)
{
    uint8_t data[2];
    int rc;

    rc = get_registers(bma253, REG_ADDR_OFC_CTRL,
                       data, sizeof(data) / sizeof(*data));
    if (rc != 0) {
        return rc;
    }

    slow_ofc_cfg->ofc_z_enabled = data[0] & 0x04;
    slow_ofc_cfg->ofc_y_enabled = data[0] & 0x02;
    slow_ofc_cfg->ofc_x_enabled = data[0] & 0x01;
    slow_ofc_cfg->high_bw_cut_off = data[1] & 0x01;

    return 0;
}

int
bma253_set_slow_ofc_cfg(const struct bma253 * bma253,
                        const struct slow_ofc_cfg * slow_ofc_cfg)
{
    uint8_t data[2];
    int rc;

    data[0] = (slow_ofc_cfg->ofc_z_enabled << 2) |
              (slow_ofc_cfg->ofc_y_enabled << 1) |
              (slow_ofc_cfg->ofc_x_enabled << 0);
    data[1] = slow_ofc_cfg->high_bw_cut_off << 0;

    rc = set_register(bma253, REG_ADDR_OFC_SETTING, data[1]);
    if (rc != 0) {
        return rc;
    }
    rc = set_register(bma253, REG_ADDR_OFC_CTRL, data[0]);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_set_ofc_reset(const struct bma253 * bma253)
{
    return set_register(bma253, REG_ADDR_OFC_CTRL, 0x80);
}

int
bma253_get_ofc_offset(const struct bma253 * bma253,
                      enum axis axis,
                      float * offset_g)
{
    uint8_t reg_addr;
    uint8_t data;
    int rc;

    switch (axis) {
    case AXIS_X:
        reg_addr = REG_ADDR_OFC_OFFSET_X;
        break;
    case AXIS_Y:
        reg_addr = REG_ADDR_OFC_OFFSET_Y;
        break;
    case AXIS_Z:
        reg_addr = REG_ADDR_OFC_OFFSET_Z;
        break;
    default:
        return SYS_EINVAL;
    }

    rc = get_register(bma253, reg_addr, &data);
    if (rc != 0) {
        return rc;
    }

    *offset_g = (float)(int8_t)data * 0.00781;

    return 0;
}

int
bma253_set_ofc_offset(const struct bma253 * bma253,
                      enum axis axis,
                      float offset_g)
{
    uint8_t reg_addr;
    uint8_t data;

    switch (axis) {
    case AXIS_X:
        reg_addr = REG_ADDR_OFC_OFFSET_X;
        break;
    case AXIS_Y:
        reg_addr = REG_ADDR_OFC_OFFSET_Y;
        break;
    case AXIS_Z:
        reg_addr = REG_ADDR_OFC_OFFSET_Z;
        break;
    default:
        return SYS_EINVAL;
    }

    data = (int8_t)(offset_g / 0.00781);

    return set_register(bma253, reg_addr, data);
}

int
bma253_get_saved_data(const struct bma253 * bma253,
                      enum saved_data_addr saved_data_addr,
                      uint8_t * saved_data_val)
{
    uint8_t reg_addr;

    switch (saved_data_addr) {
    case SAVED_DATA_ADDR_0:
        reg_addr = REG_ADDR_TRIM_GP0;
        break;
    case SAVED_DATA_ADDR_1:
        reg_addr = REG_ADDR_TRIM_GP1;
        break;
    default:
        return SYS_EINVAL;
    }

    return get_register(bma253, reg_addr, saved_data_val);
}

int
bma253_set_saved_data(const struct bma253 * bma253,
                      enum saved_data_addr saved_data_addr,
                      uint8_t saved_data_val)
{
    uint8_t reg_addr;

    switch (saved_data_addr) {
    case SAVED_DATA_ADDR_0:
        reg_addr = REG_ADDR_TRIM_GP0;
        break;
    case SAVED_DATA_ADDR_1:
        reg_addr = REG_ADDR_TRIM_GP1;
        break;
    default:
        return SYS_EINVAL;
    }

    return set_register(bma253, reg_addr, saved_data_val);
}

int
bma253_get_fifo_cfg(const struct bma253 * bma253,
                    struct fifo_cfg * fifo_cfg)
{
    uint8_t data;
    int rc;

    rc = get_register(bma253, REG_ADDR_FIFO_CONFIG_1, &data);
    if (rc != 0) {
        return rc;
    }

    switch ((data >> 6) & 0x03) {
    case 0x03:
        BMA253_ERROR("unknown FIFO_CONFIG_1 reg value 0x%02X\n", data);
    case 0x00:
        fifo_cfg->fifo_mode = FIFO_MODE_BYPASS;
        break;
    case 0x01:
        fifo_cfg->fifo_mode = FIFO_MODE_FIFO;
        break;
    case 0x02:
        fifo_cfg->fifo_mode = FIFO_MODE_STREAM;
        break;
    }

    switch ((data >> 0) & 0x03) {
    case 0x00:
        fifo_cfg->fifo_data = FIFO_DATA_X_AND_Y_AND_Z;
        break;
    case 0x01:
        fifo_cfg->fifo_data = FIFO_DATA_X_ONLY;
        break;
    case 0x02:
        fifo_cfg->fifo_data = FIFO_DATA_Y_ONLY;
        break;
    case 0x03:
        fifo_cfg->fifo_data = FIFO_DATA_Z_ONLY;
        break;
    }

    return 0;
}

int
bma253_set_fifo_cfg(const struct bma253 * bma253,
                    const struct fifo_cfg * fifo_cfg)
{
    uint8_t data;

    data = 0;

    switch (fifo_cfg->fifo_mode) {
    case FIFO_MODE_BYPASS:
        data |= 0x00 << 6;
        break;
    case FIFO_MODE_FIFO:
        data |= 0x01 << 6;
        break;
    case FIFO_MODE_STREAM:
        data |= 0x02 << 6;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (fifo_cfg->fifo_data) {
    case FIFO_DATA_X_AND_Y_AND_Z:
        data |= 0x00 << 0;
        break;
    case FIFO_DATA_X_ONLY:
        data |= 0x01 << 0;
        break;
    case FIFO_DATA_Y_ONLY:
        data |= 0x02 << 0;
        break;
    case FIFO_DATA_Z_ONLY:
        data |= 0x03 << 0;
        break;
    }

    return set_register(bma253, REG_ADDR_FIFO_CONFIG_1, data);
}

int
bma253_get_fifo(const struct bma253 * bma253,
                enum bma253_g_range g_range,
                enum fifo_data fifo_data,
                struct accel_data * accel_data)
{
    float accel_scale;
    uint8_t size, iter;
    uint8_t data[AXIS_ALL << 1];
    int rc;

    switch (g_range) {
    case BMA253_G_RANGE_2:
        accel_scale = 0.00098;
        break;
    case BMA253_G_RANGE_4:
        accel_scale = 0.00195;
        break;
    case BMA253_G_RANGE_8:
        accel_scale = 0.00391;
        break;
    case BMA253_G_RANGE_16:
        accel_scale = 0.00781;
        break;
    default:
        return SYS_EINVAL;
    }

    switch (fifo_data) {
    case FIFO_DATA_X_AND_Y_AND_Z:
        size = AXIS_ALL << 1;
        break;
    case FIFO_DATA_X_ONLY:
    case FIFO_DATA_Y_ONLY:
    case FIFO_DATA_Z_ONLY:
        size = 1 << 1;
        break;
    default:
        return SYS_EINVAL;
    }

    rc = get_registers(bma253, REG_ADDR_FIFO_DATA, data, size);
    if (rc != 0) {
        return rc;
    }

    for (iter = 0; iter < size; iter += 2) {
        compute_accel_data(accel_data + (iter >> 1),
                           data + iter,
                           accel_scale);
    }

    return 0;
}

static int
reset_and_recfg(struct bma253 * bma253)
{
    const struct bma253_cfg * cfg;
    int rc;
    enum int_route int_route;
    struct int_routes int_routes;
    struct int_filters int_filters;
    struct int_pin_electrical int_pin_electrical;
    struct low_g_int_cfg low_g_int_cfg;
    struct high_g_int_cfg high_g_int_cfg;
    struct tap_int_cfg tap_int_cfg;
    struct orient_int_cfg orient_int_cfg;
    enum i2c_watchdog i2c_watchdog;
    struct fifo_cfg fifo_cfg;
    struct bma253_private_driver_data *pdd;

    cfg = &bma253->cfg;
    pdd = &bma253->pdd;

    bma253->power = BMA253_POWER_MODE_NORMAL;

    rc = bma253_set_softreset(bma253);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_g_range(bma253, cfg->g_range);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_filter_bandwidth(bma253, cfg->filter_bandwidth);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_data_acquisition(bma253,
                     cfg->use_unfiltered_data,
                     false);
    if (rc != 0) {
        return rc;
    }

    int_route = INT_ROUTE_NONE;
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    int_route = pdd->int_route;
#endif

    int_routes.flat_int_route        = INT_ROUTE_NONE;
    int_routes.orient_int_route      = int_route;
    int_routes.s_tap_int_route       = INT_ROUTE_NONE;
    int_routes.d_tap_int_route       = INT_ROUTE_NONE;
    int_routes.slow_no_mot_int_route = INT_ROUTE_NONE;
    int_routes.slope_int_route       = INT_ROUTE_NONE;
    int_routes.high_g_int_route      = int_route;
    int_routes.low_g_int_route       = int_route;
    int_routes.fifo_wmark_int_route  = INT_ROUTE_NONE;
    int_routes.fifo_full_int_route   = INT_ROUTE_NONE;
    int_routes.data_int_route        = int_route;

    rc = bma253_set_int_routes(bma253, &int_routes);
    if (rc != 0) {
        return rc;
    }

    int_filters.unfiltered_data_int        = cfg->use_unfiltered_data;
    int_filters.unfiltered_tap_int         = cfg->use_unfiltered_data;
    int_filters.unfiltered_slow_no_mot_int = cfg->use_unfiltered_data;
    int_filters.unfiltered_slope_int       = cfg->use_unfiltered_data;
    int_filters.unfiltered_high_g_int      = cfg->use_unfiltered_data;
    int_filters.unfiltered_low_g_int       = cfg->use_unfiltered_data;

    rc = bma253_set_int_filters(bma253, &int_filters);
    if (rc != 0) {
        return rc;
    }

#if MYNEWT_VAL(BMA253_INT_CFG_OUTPUT)
    int_pin_electrical.pin1_output = INT_PIN_OUTPUT_OPEN_DRAIN;
    int_pin_electrical.pin2_output = INT_PIN_OUTPUT_OPEN_DRAIN;
#else
    int_pin_electrical.pin1_output = INT_PIN_OUTPUT_PUSH_PULL;
    int_pin_electrical.pin2_output = INT_PIN_OUTPUT_PUSH_PULL;
#endif
#if MYNEWT_VAL(BMA253_INT_CFG_ACTIVE)
    int_pin_electrical.pin1_active = INT_PIN_ACTIVE_HIGH;
    int_pin_electrical.pin2_active = INT_PIN_ACTIVE_HIGH;
#else
    int_pin_electrical.pin1_active = INT_PIN_ACTIVE_LOW;
    int_pin_electrical.pin2_active = INT_PIN_ACTIVE_LOW;
#endif

    rc = bma253_set_int_pin_electrical(bma253, &int_pin_electrical);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_int_latch(bma253, false, INT_LATCH_NON_LATCHED);
    if (rc != 0) {
        return rc;
    }

    low_g_int_cfg.delay_ms     = cfg->low_g_delay_ms;
    low_g_int_cfg.thresh_g     = cfg->low_g_thresh_g;
    low_g_int_cfg.hyster_g     = cfg->low_g_hyster_g;
    low_g_int_cfg.axis_summing = false;

    rc = bma253_set_low_g_int_cfg(bma253, &low_g_int_cfg);
    if (rc != 0) {
        return rc;
    }

    high_g_int_cfg.hyster_g = cfg->high_g_hyster_g;
    high_g_int_cfg.delay_ms = cfg->high_g_delay_ms;
    high_g_int_cfg.thresh_g = cfg->high_g_thresh_g;

    rc = bma253_set_high_g_int_cfg(bma253, cfg->g_range, &high_g_int_cfg);
    if (rc != 0) {
        return rc;
    }

    tap_int_cfg.tap_quiet        = cfg->tap_quiet;
    tap_int_cfg.tap_shock        = cfg->tap_shock;
    tap_int_cfg.d_tap_window     = cfg->d_tap_window;
    tap_int_cfg.tap_wake_samples = cfg->tap_wake_samples;
    tap_int_cfg.thresh_g         = cfg->tap_thresh_g;

    rc = bma253_set_tap_int_cfg(bma253, cfg->g_range, &tap_int_cfg);
    if (rc != 0) {
        return rc;
    }

    orient_int_cfg.hyster_g        = cfg->orient_hyster_g;
    orient_int_cfg.orient_blocking = cfg->orient_blocking;
    orient_int_cfg.orient_mode     = cfg->orient_mode;
    orient_int_cfg.signal_up_dn    = cfg->orient_signal_ud;
    orient_int_cfg.blocking_angle  = 0x08;

    rc = bma253_set_orient_int_cfg(bma253, &orient_int_cfg);
    if (rc != 0) {
        return rc;
    }

#if MYNEWT_VAL(BMA253_I2C_WDT)
    i2c_watchdog = I2C_WATCHDOG_50_MS;
#else
    i2c_watchdog = I2C_WATCHDOG_DISABLED;
#endif

    rc = bma253_set_i2c_watchdog(bma253, i2c_watchdog);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_ofc_offset(bma253, AXIS_X, cfg->offset_x_g);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_ofc_offset(bma253, AXIS_Y, cfg->offset_y_g);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_ofc_offset(bma253, AXIS_Z, cfg->offset_z_g);
    if (rc != 0) {
        return rc;
    }

    fifo_cfg.fifo_mode = FIFO_MODE_BYPASS;
    fifo_cfg.fifo_data = FIFO_DATA_X_AND_Y_AND_Z;

    rc = bma253_set_fifo_cfg(bma253, &fifo_cfg);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
change_power(struct bma253 * bma253,
             enum bma253_power_mode target)
{
    const struct bma253_cfg * cfg;
    int rc;
    bool step1_move;
    bool step2_move;
    enum bma253_power_mode step1_mode;
    enum bma253_power_mode step2_mode;
    struct power_settings power_settings;

    cfg = &bma253->cfg;

    if (bma253->power == BMA253_POWER_MODE_DEEP_SUSPEND) {
        rc = reset_and_recfg(bma253);
        if (rc != 0) {
            return rc;
        }
    }

    step1_move = false;
    switch (bma253->power) {
    case BMA253_POWER_MODE_SUSPEND:
    case BMA253_POWER_MODE_LPM_1:
        switch (target) {
        case BMA253_POWER_MODE_STANDBY:
        case BMA253_POWER_MODE_LPM_2:
            step1_mode = BMA253_POWER_MODE_NORMAL;
            step1_move = true;
            break;
        default:
            break;
        }
        break;
    case BMA253_POWER_MODE_STANDBY:
    case BMA253_POWER_MODE_LPM_2:
        switch (target) {
        case BMA253_POWER_MODE_SUSPEND:
        case BMA253_POWER_MODE_LPM_1:
            step1_mode = BMA253_POWER_MODE_NORMAL;
            step1_move = true;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    if (bma253->power != target) {
        step2_mode = target;
        step2_move = true;
    } else {
        step2_move = false;
    }

    if (step1_move) {
        power_settings.power_mode     = step1_mode;
        power_settings.sleep_duration = cfg->sleep_duration;
        power_settings.sleep_timer    = SLEEP_TIMER_EVENT_DRIVEN;

        rc = bma253_set_power_settings(bma253, &power_settings);
        if (rc != 0) {
            return rc;
        }

        bma253->power = step1_mode;
    }

    if (step2_move) {
        power_settings.power_mode     = step2_mode;
        power_settings.sleep_duration = cfg->sleep_duration;
        power_settings.sleep_timer    = SLEEP_TIMER_EVENT_DRIVEN;

        rc = bma253_set_power_settings(bma253, &power_settings);
        if (rc != 0) {
            return rc;
        }

        bma253->power = step2_mode;
    }

    return 0;
}

static int
interim_power(struct bma253 * bma253,
              const enum bma253_power_mode * reqs,
              uint8_t size)
{
    uint8_t i;

    if (size == 0) {
        return SYS_EINVAL;
    }

    for (i = 0; i < size; i++) {
        if (reqs[i] == bma253->power) {
            return 0;
        }
    }

    return change_power(bma253, reqs[0]);
}

static int
default_power(struct bma253 * bma253)
{
    const struct bma253_cfg * cfg;

    cfg = &bma253->cfg;

    if (cfg->power_mode == bma253->power) {
        return 0;
    }

    return change_power(bma253, cfg->power_mode);
}

#if MYNEWT_VAL(BMA253_INT_ENABLE)
static int
init_intpin(struct bma253 * bma253,
              hal_gpio_irq_handler_t handler,
              void * arg)
{
    struct bma253_private_driver_data *pdd = &bma253->pdd;
    hal_gpio_irq_trig_t trig;
    int pin = -1;
    int rc;
    int i;

    for (i = 0; i < MYNEWT_VAL(SENSOR_MAX_INTERRUPTS_PINS); i++){
        pin = bma253->sensor.s_itf.si_ints[i].host_pin;
        if (pin > 0) {
            break;
        }
    }

    if (pin < 0) {
        BMA253_ERROR("Interrupt pin not configured\n");
        return SYS_EINVAL;
    }

    pdd->int_num = i;
    if (bma253->sensor.s_itf.si_ints[pdd->int_num].active) {
        trig = HAL_GPIO_TRIG_RISING;
    } else {
        trig = HAL_GPIO_TRIG_FALLING;
    }

    if (bma253->sensor.s_itf.si_ints[pdd->int_num].device_pin == 1) {
        pdd->int_route = INT_ROUTE_PIN_1;
    } else if (bma253->sensor.s_itf.si_ints[pdd->int_num].device_pin == 2) {
        pdd->int_route = INT_ROUTE_PIN_2;
    } else {
        BMA253_ERROR("Route not configured\n");
        return SYS_EINVAL;
    }

    rc = hal_gpio_irq_init(pin,
                           handler,
                           arg,
                           trig,
                           HAL_GPIO_PULL_NONE);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static void
enable_intpin(struct bma253 * bma253)
{
    struct bma253_private_driver_data *pdd = &bma253->pdd;
    enum bma253_int_num int_num = pdd->int_num;

    pdd->int_ref_cnt++;

    if (pdd->int_ref_cnt == 1) {
        hal_gpio_irq_enable(bma253->sensor.s_itf.si_ints[int_num].host_pin);
    }
}

static void
disable_intpin(struct bma253 * bma253)
{
    struct bma253_private_driver_data *pdd = &bma253->pdd;
    enum bma253_int_num int_num = pdd->int_num;

    if (pdd->int_ref_cnt == 0) {
        return;
    }

    pdd->int_ref_cnt--;

    if (pdd->int_ref_cnt == 0) {
        hal_gpio_irq_disable(bma253->sensor.s_itf.si_ints[int_num].host_pin);
    }
}
#endif

static int
self_test_enable(const struct bma253 * bma253,
                 enum self_test_ampl ampl,
                 enum self_test_sign sign,
                 enum axis axis)
{
    struct self_test_cfg self_test_cfg;

    self_test_cfg.self_test_ampl    = ampl;
    self_test_cfg.self_test_sign    = sign;
    self_test_cfg.self_test_axis    = axis;
    self_test_cfg.self_test_enabled = true;

    return bma253_set_self_test_cfg(bma253, &self_test_cfg);
}

static int
self_test_disable(const struct bma253 * bma253)
{
    struct self_test_cfg self_test_cfg;

    self_test_cfg.self_test_ampl    = SELF_TEST_AMPL_LOW;
    self_test_cfg.self_test_sign    = SELF_TEST_SIGN_NEGATIVE;
    self_test_cfg.self_test_axis    = -1;
    self_test_cfg.self_test_enabled = false;

    return bma253_set_self_test_cfg(bma253, &self_test_cfg);
}

static int
self_test_nudge(const struct bma253 * bma253,
                enum self_test_ampl ampl,
                enum self_test_sign sign,
                enum axis axis,
                enum bma253_g_range g_range,
                struct accel_data * accel_data)
{
    int rc;

    rc = self_test_enable(bma253, ampl, sign, axis);
    if (rc != 0) {
        return rc;
    }

    delay_msec(50);

    rc = bma253_get_accel(bma253, g_range, axis, accel_data);
    if (rc != 0) {
        return rc;
    }

    rc = self_test_disable(bma253);
    if (rc != 0) {
        return rc;
    }

    delay_msec(50);

    return 0;
}

static int
self_test_axis(const struct bma253 * bma253,
               enum axis axis,
               enum bma253_g_range g_range,
               float * delta_hi_g,
               float * delta_lo_g)
{
    struct accel_data accel_neg_hi;
    struct accel_data accel_neg_lo;
    struct accel_data accel_pos_hi;
    struct accel_data accel_pos_lo;
    int rc;

    rc = self_test_nudge(bma253,
                         SELF_TEST_AMPL_HIGH,
                         SELF_TEST_SIGN_NEGATIVE,
                         axis,
                         g_range,
                         &accel_neg_hi);
    if (rc != 0) {
        return rc;
    }
    rc = self_test_nudge(bma253,
                         SELF_TEST_AMPL_LOW,
                         SELF_TEST_SIGN_NEGATIVE,
                         axis,
                         g_range,
                         &accel_neg_lo);
    if (rc != 0) {
        return rc;
    }
    rc = self_test_nudge(bma253,
                         SELF_TEST_AMPL_HIGH,
                         SELF_TEST_SIGN_POSITIVE,
                         axis,
                         g_range,
                         &accel_pos_hi);
    if (rc != 0) {
        return rc;
    }
    rc = self_test_nudge(bma253,
                         SELF_TEST_AMPL_LOW,
                         SELF_TEST_SIGN_POSITIVE,
                         axis,
                         g_range,
                         &accel_pos_lo);
    if (rc != 0) {
        return rc;
    }

    *delta_hi_g = accel_pos_hi.accel_g - accel_neg_hi.accel_g;
    *delta_lo_g = accel_pos_lo.accel_g - accel_neg_lo.accel_g;

    return 0;
}

int
bma253_self_test(struct bma253 * bma253,
                 float delta_high_mult,
                 float delta_low_mult,
                 bool * self_test_fail)
{
    const struct bma253_cfg * cfg;
    enum bma253_power_mode request_power;
    int rc;
    float delta_hi_x_g;
    float delta_lo_x_g;
    float delta_hi_y_g;
    float delta_lo_y_g;
    float delta_hi_z_g;
    float delta_lo_z_g;
    bool fail;

    cfg = &bma253->cfg;

    request_power = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253, &request_power, 1);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_g_range(bma253, BMA253_G_RANGE_8);
    if (rc != 0) {
        return rc;
    }

    rc = self_test_axis(bma253,
                        AXIS_X,
                        BMA253_G_RANGE_8,
                        &delta_hi_x_g,
                        &delta_lo_x_g);
    if (rc != 0) {
        return rc;
    }
    rc = self_test_axis(bma253,
                        AXIS_Y,
                        BMA253_G_RANGE_8,
                        &delta_hi_y_g,
                        &delta_lo_y_g);
    if (rc != 0) {
        return rc;
    }
    rc = self_test_axis(bma253,
                        AXIS_Z,
                        BMA253_G_RANGE_8,
                        &delta_hi_z_g,
                        &delta_lo_z_g);
    if (rc != 0) {
        return rc;
    }

    rc = self_test_disable(bma253);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_g_range(bma253, cfg->g_range);
    if (rc != 0) {
        return rc;
    }

    delay_msec(50);

    rc = default_power(bma253);
    if (rc != 0) {
        return rc;
    }

    fail = false;
    if (delta_hi_x_g < delta_high_mult * 0.8) {
        fail = true;
    }
    if (delta_lo_x_g < delta_low_mult * 0.8) {
        fail = true;
    }
    if (delta_hi_y_g < delta_high_mult * 0.8) {
        fail = true;
    }
    if (delta_lo_y_g < delta_low_mult * 0.8) {
        fail = true;
    }
    if (delta_hi_z_g < delta_high_mult * 0.4) {
        fail = true;
    }
    if (delta_lo_z_g < delta_low_mult * 0.4) {
        fail = true;
    }

    *self_test_fail = fail;

    return 0;
}

static int
axis_offset_compensation(const struct bma253 * bma253,
                         enum axis axis,
                         enum bma253_offset_comp_target target)
{
    int rc;
    bool ready;
    enum bma253_offset_comp_target target_z;
    enum bma253_offset_comp_target target_y;
    enum bma253_offset_comp_target target_x;
    uint32_t count;

    rc = bma253_get_fast_ofc_cfg(bma253,
                                 &ready,
                                 &target_z,
                                 &target_y,
                                 &target_x);
    if (rc != 0) {
        return rc;
    }

    if (!ready) {
        BMA253_ERROR("offset compensation already in progress\n");
        return SYS_ETIMEOUT;
    }

    rc = bma253_set_fast_ofc_cfg(bma253, axis, target, true);
    if (rc != 0) {
        return rc;
    }

    for (count = 1000; count != 0; count--) {
        rc = bma253_get_fast_ofc_cfg(bma253,
                                     &ready,
                                     &target_z,
                                     &target_y,
                                     &target_x);
        if (rc != 0) {
            return rc;
        }

        if (ready) {
            break;
        }
    }

    if (count == 0) {
        BMA253_ERROR("offset compensation did not complete\n");
        return SYS_ETIMEOUT;
    }

    return 0;
}

int
bma253_offset_compensation(struct bma253 * bma253,
                           enum bma253_offset_comp_target target_x,
                           enum bma253_offset_comp_target target_y,
                           enum bma253_offset_comp_target target_z)
{
    const struct bma253_cfg * cfg;
    enum bma253_power_mode request_power;
    int rc;

    cfg = &bma253->cfg;

    request_power = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253, &request_power, 1);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_g_range(bma253, BMA253_G_RANGE_2);
    if (rc != 0) {
        return rc;
    }

    rc = axis_offset_compensation(bma253, AXIS_X, target_x);
    if (rc != 0) {
        return rc;
    }
    rc = axis_offset_compensation(bma253, AXIS_Y, target_y);
    if (rc != 0) {
        return rc;
    }
    rc = axis_offset_compensation(bma253, AXIS_Z, target_z);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_get_ofc_offset(bma253, AXIS_X, &bma253->cfg.offset_x_g);
    if (rc != 0) {
        return rc;
    }
    rc = bma253_get_ofc_offset(bma253, AXIS_Y, &bma253->cfg.offset_y_g);
    if (rc != 0) {
        return rc;
    }
    rc = bma253_get_ofc_offset(bma253, AXIS_Z, &bma253->cfg.offset_z_g);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_g_range(bma253, cfg->g_range);
    if (rc != 0) {
        return rc;
    }

    rc = default_power(bma253);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_query_offsets(struct bma253 * bma253,
                     float * offset_x_g,
                     float * offset_y_g,
                     float * offset_z_g)
{
    const struct bma253_cfg * cfg;
    enum bma253_power_mode request_power[5];
    int rc;
    float val_offset_x_g;
    float val_offset_y_g;
    float val_offset_z_g;
    bool mismatch;

    cfg = &bma253->cfg;

    request_power[0] = BMA253_POWER_MODE_SUSPEND;
    request_power[1] = BMA253_POWER_MODE_STANDBY;
    request_power[2] = BMA253_POWER_MODE_LPM_1;
    request_power[3] = BMA253_POWER_MODE_LPM_2;
    request_power[4] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        return rc;
    }

    rc = bma253_get_ofc_offset(bma253, AXIS_X, &val_offset_x_g);
    if (rc != 0) {
        return rc;
    }
    rc = bma253_get_ofc_offset(bma253, AXIS_Y, &val_offset_y_g);
    if (rc != 0) {
        return rc;
    }
    rc = bma253_get_ofc_offset(bma253, AXIS_Z, &val_offset_z_g);
    if (rc != 0) {
        return rc;
    }

    rc = default_power(bma253);
    if (rc != 0) {
        return rc;
    }

    mismatch = false;
    if (cfg->offset_x_g != val_offset_x_g) {
        BMA253_ERROR("X compensation offset value mismatch\n");
        mismatch = true;
    }
    if (cfg->offset_y_g != val_offset_y_g) {
        BMA253_ERROR("Y compensation offset value mismatch\n");
        mismatch = true;
    }
    if (cfg->offset_z_g != val_offset_z_g) {
        BMA253_ERROR("Z compensation offset value mismatch\n");
        mismatch = true;
    }

    if (mismatch) {
        return SYS_EINVAL;
    }

    *offset_x_g = val_offset_x_g;
    *offset_y_g = val_offset_y_g;
    *offset_z_g = val_offset_z_g;

    return 0;
}

int
bma253_write_offsets(struct bma253 * bma253,
                     float offset_x_g,
                     float offset_y_g,
                     float offset_z_g)
{
    struct bma253_cfg * cfg;
    enum bma253_power_mode request_power[5];
    int rc;

    cfg = &bma253->cfg;

    request_power[0] = BMA253_POWER_MODE_SUSPEND;
    request_power[1] = BMA253_POWER_MODE_STANDBY;
    request_power[2] = BMA253_POWER_MODE_LPM_1;
    request_power[3] = BMA253_POWER_MODE_LPM_2;
    request_power[4] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        return rc;
    }

    rc = bma253_set_ofc_offset(bma253, AXIS_X, offset_x_g);
    if (rc != 0) {
        return rc;
    }
    rc = bma253_set_ofc_offset(bma253, AXIS_Y, offset_y_g);
    if (rc != 0) {
        return rc;
    }
    rc = bma253_set_ofc_offset(bma253, AXIS_Z, offset_z_g);
    if (rc != 0) {
        return rc;
    }

    cfg->offset_x_g = offset_x_g;
    cfg->offset_y_g = offset_y_g;
    cfg->offset_z_g = offset_z_g;

    return 0;
}

int
bma253_stream_read(struct sensor *sensor,
                   sensor_type_t sensor_type,
                   sensor_data_func_t read_func,
                   void *read_arg,
                   uint32_t time_ms)
{
    struct bma253 *bma253;
    const struct bma253_cfg * cfg;
    int rc;
    enum bma253_power_mode request_power;
    struct int_enable int_enable_org;
    struct int_enable int_enable = { 0 };
    os_time_t time_ticks;
    os_time_t stop_ticks;
    struct accel_data accel_data[AXIS_ALL];
    struct sensor_accel_data sad;
    struct bma253_private_driver_data *pdd;

    if ((sensor_type & ~(SENSOR_TYPE_ACCELEROMETER |
                         SENSOR_TYPE_AMBIENT_TEMPERATURE)) != 0) {
        return SYS_EINVAL;
    }

    bma253 = (struct bma253 *)SENSOR_GET_DEVICE(sensor);

    cfg = &bma253->cfg;
    pdd = &bma253->pdd;

    stop_ticks = 0;

    request_power = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253, &request_power, 1);
    if (rc != 0) {
        return rc;
    }

#if MYNEWT_VAL(BMA253_INT_ENABLE)
    undo_interrupt(&bma253->intr);

    if (pdd->interrupt) {
        return SYS_EBUSY;
    }
    pdd->interrupt = &bma253->intr;
    enable_intpin(bma253);
#endif

    rc = bma253_get_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        goto done;
    }

    /* Leave tap configured as it is since it is on int2*/
    int_enable.s_tap_int_enable = int_enable_org.s_tap_int_enable;
    int_enable.d_tap_int_enable = int_enable_org.d_tap_int_enable;
    int_enable.data_int_enable = true;

    rc = bma253_set_int_enable(bma253, &int_enable);
    if (rc != 0) {
        goto done;
    }

    if (time_ms != 0) {
        rc = os_time_ms_to_ticks(time_ms, &time_ticks);
        if (rc != 0) {
            goto done;
        }
        stop_ticks = os_time_get() + time_ticks;
    }

    for (;;) {
#if MYNEWT_VAL(BMA253_INT_ENABLE)
        wait_interrupt(&bma253->intr, pdd->int_num);
#else
        switch (cfg->filter_bandwidth) {
        case BMA253_FILTER_BANDWIDTH_7_81_HZ:
            delay_msec(128);
            break;
        case BMA253_FILTER_BANDWIDTH_15_63_HZ:
            delay_msec(64);
            break;
        case BMA253_FILTER_BANDWIDTH_31_25_HZ:
            delay_msec(32);
            break;
        case BMA253_FILTER_BANDWIDTH_62_5_HZ:
            delay_msec(16);
            break;
        case BMA253_FILTER_BANDWIDTH_125_HZ:
            delay_msec(8);
            break;
        case BMA253_FILTER_BANDWIDTH_250_HZ:
            delay_msec(4);
            break;
        case BMA253_FILTER_BANDWIDTH_500_HZ:
            delay_msec(2);
            break;
        case BMA253_FILTER_BANDWIDTH_1000_HZ:
            delay_msec(1);
            break;
        default:
            delay_msec(1000);
            break;
        }
#endif

        rc = bma253_get_fifo(bma253,
                             cfg->g_range,
                             FIFO_DATA_X_AND_Y_AND_Z,
                             accel_data);
        if (rc != 0) {
            goto done;
        }

        sad.sad_x = accel_data[AXIS_X].accel_g;
        sad.sad_y = accel_data[AXIS_Y].accel_g;
        sad.sad_z = accel_data[AXIS_Z].accel_g;
        sad.sad_x_is_valid = 1;
        sad.sad_y_is_valid = 1;
        sad.sad_z_is_valid = 1;

        if (read_func(sensor, read_arg, &sad, SENSOR_TYPE_ACCELEROMETER)) {
            break;
        }

        if (time_ms != 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
                break;
        }
    }

    rc = bma253_set_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        goto done;
    }

    rc = default_power(bma253);
    if (rc != 0) {
        goto done;
    }

done:
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    pdd->interrupt = NULL;
    disable_intpin(bma253);
#endif

    return rc;
}

int
bma253_current_temp(struct bma253 * bma253,
                    float * temp_c)
{
    enum bma253_power_mode request_power[3];
    int rc;

    request_power[0] = BMA253_POWER_MODE_LPM_1;
    request_power[1] = BMA253_POWER_MODE_LPM_2;
    request_power[2] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        return rc;
    }

    rc = bma253_get_temp(bma253, temp_c);
    if (rc != 0) {
        return rc;
    }

    rc = default_power(bma253);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_current_orient(struct bma253 * bma253,
                      struct bma253_orient_xyz * orient_xyz)
{
    enum bma253_power_mode request_power[3];
    int rc;
    struct int_enable int_enable_org;
    struct int_enable int_enable = { 0 };
    struct int_status int_status;

    request_power[0] = BMA253_POWER_MODE_LPM_1;
    request_power[1] = BMA253_POWER_MODE_LPM_2;
    request_power[2] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        return rc;
    }

    rc = bma253_get_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        return rc;
    }

   /* Leave tap configured as it is since it is on int2*/
    int_enable.s_tap_int_enable = int_enable_org.s_tap_int_enable;
    int_enable.d_tap_int_enable = int_enable_org.d_tap_int_enable;

    int_enable.orient_int_enable = true;

    rc = bma253_set_int_enable(bma253, &int_enable);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_get_int_status(bma253, &int_status);
    if (rc != 0) {
        return rc;
    }

    /* Back to original interrupts */
    rc = bma253_set_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        return rc;
    }

    rc = default_power(bma253);
    if (rc != 0) {
        return rc;
    }

    orient_xyz->orient_xy  = int_status.device_orientation;
    orient_xyz->downward_z = int_status.device_is_down;

    return 0;
}

int
bma253_wait_for_orient(struct bma253 * bma253,
                       struct bma253_orient_xyz * orient_xyz)
{
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    int rc;
    enum bma253_power_mode request_power[3];
    struct int_enable int_enable_org;
    struct int_enable int_enable = {0};
    struct int_status int_status;
    struct bma253_private_driver_data *pdd;

    pdd = &bma253->pdd;

    if (pdd->interrupt) {
        BMA253_ERROR("Interrupt used\n");
        return SYS_EINVAL;
    }

    pdd->interrupt = &bma253->intr;
    enable_intpin(bma253);

    request_power[0] = BMA253_POWER_MODE_LPM_1;
    request_power[1] = BMA253_POWER_MODE_LPM_2;
    request_power[2] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        goto done;
    }

    undo_interrupt(&bma253->intr);

    rc = bma253_get_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        return rc;
    }

   /* Leave tap configured as it is since it is on int2*/
    int_enable.s_tap_int_enable = int_enable_org.s_tap_int_enable;
    int_enable.d_tap_int_enable = int_enable_org.d_tap_int_enable;
    int_enable.orient_int_enable = true;
    rc = bma253_set_int_enable(bma253, &int_enable);
    if (rc != 0) {
        goto done;
    }

    wait_interrupt(&bma253->intr, pdd->int_num);

    rc = bma253_get_int_status(bma253, &int_status);
    if (rc != 0) {
        goto done;
    }

    /* Back to original interrupts */
    rc = bma253_set_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        goto done;
    }

    rc = default_power(bma253);
    if (rc != 0) {
        goto done;
    }

    orient_xyz->orient_xy  = int_status.device_orientation;
    orient_xyz->downward_z = int_status.device_is_down;

done:
    pdd->interrupt = NULL;
    disable_intpin(bma253);
    return rc;
#else
    return SYS_ENODEV;
#endif
}

int
bma253_wait_for_high_g(struct bma253 * bma253)
{
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    int rc;
    enum bma253_power_mode request_power[3];
    struct int_enable int_enable_org;
    struct int_enable int_enable = { 0 };
    struct bma253_private_driver_data *pdd;

    pdd = &bma253->pdd;

    if (pdd->interrupt) {
        BMA253_ERROR("Interrupt used\n");
        return SYS_EINVAL;
    }

    pdd->interrupt = &bma253->intr;
    enable_intpin(bma253);

    request_power[0] = BMA253_POWER_MODE_LPM_1;
    request_power[1] = BMA253_POWER_MODE_LPM_2;
    request_power[2] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        goto done;
    }

    undo_interrupt(&bma253->intr);

    rc = bma253_get_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        return rc;
    }

   /* Leave tap configured as it is since it is on int2*/
    int_enable.s_tap_int_enable = int_enable_org.s_tap_int_enable;
    int_enable.d_tap_int_enable = int_enable_org.d_tap_int_enable;

    int_enable.high_g_z_int_enable = true;
    int_enable.high_g_y_int_enable = true;
    int_enable.high_g_x_int_enable = true;

    rc = bma253_set_int_enable(bma253, &int_enable);
    if (rc != 0) {
        goto done;
    }

    wait_interrupt(&bma253->intr, pdd->int_num);

    rc = bma253_set_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        goto done;
    }

    rc = default_power(bma253);
    if (rc != 0) {
        goto done;
    }

done:
    pdd->interrupt = NULL;
    disable_intpin(bma253);
    return rc;
#else
    return SYS_ENODEV;
#endif
}

int
bma253_wait_for_low_g(struct bma253 * bma253)
{
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    int rc;
    enum bma253_power_mode request_power[3];
    struct int_enable int_enable_org;
    struct int_enable int_enable = { 0 };
    struct bma253_private_driver_data *pdd;

    pdd = &bma253->pdd;

    if (pdd->interrupt) {
        BMA253_ERROR("Interrupt used\n");
        return SYS_EINVAL;
    }

    pdd->interrupt = &bma253->intr;
    enable_intpin(bma253);

    request_power[0] = BMA253_POWER_MODE_LPM_1;
    request_power[1] = BMA253_POWER_MODE_LPM_2;
    request_power[2] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        goto done;
    }

    undo_interrupt(&bma253->intr);

    rc = bma253_get_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        return rc;
    }

   /* Leave tap configured as it is since it is on int2 */
    int_enable.s_tap_int_enable = int_enable_org.s_tap_int_enable;
    int_enable.d_tap_int_enable = int_enable_org.d_tap_int_enable;

    int_enable.low_g_int_enable         = true;

    rc = bma253_set_int_enable(bma253, &int_enable);
    if (rc != 0) {
        goto done;
    }

    wait_interrupt(&bma253->intr, pdd->int_num);

    rc = bma253_set_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        goto done;
    }

    rc = default_power(bma253);
    if (rc != 0) {
        goto done;
    }

done:
    pdd->interrupt = NULL;
    disable_intpin(bma253);
    return 0;
#else
    return SYS_ENODEV;
#endif
}

int
bma253_wait_for_tap(struct bma253 * bma253,
                    enum bma253_tap_type tap_type)
{
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    int rc = 0;
    enum bma253_power_mode request_power[3];
    struct int_enable int_enable_org;
    struct int_enable int_enable = { 0 };
    struct int_routes int_routes;
    struct int_routes int_routes_org;
    struct bma253_private_driver_data *pdd;

    pdd = &bma253->pdd;

    switch (tap_type) {
    case BMA253_TAP_TYPE_DOUBLE:
    case BMA253_TAP_TYPE_SINGLE:
        break;
    default:
        return SYS_EINVAL;
    }

    rc = bma253_get_int_routes(bma253, &int_routes_org);
    if (rc != 0) {
        return rc;
    }

    int_routes = int_routes_org;
    if (tap_type == BMA253_TAP_TYPE_DOUBLE) {
        /* According to BMA253 when single tap shall not be used we should not
         * route it to any INTX
         */
        int_routes.d_tap_int_route = pdd->int_route;
        int_routes.s_tap_int_route = INT_ROUTE_NONE;
    } else {
        int_routes.d_tap_int_route = INT_ROUTE_NONE;
        int_routes.s_tap_int_route = pdd->int_route;
    }

    rc = bma253_set_int_routes(bma253, &int_routes);
    if (rc != 0) {
        return rc;
    }

    if (pdd->interrupt) {
        BMA253_ERROR("Interrupt used\n");
        return SYS_EINVAL;
    }

    pdd->interrupt = &bma253->intr;
    enable_intpin(bma253);

    request_power[0] = BMA253_POWER_MODE_LPM_1;
    request_power[1] = BMA253_POWER_MODE_LPM_2;
    request_power[2] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        goto done;
    }

    undo_interrupt(&bma253->intr);

    rc = bma253_get_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        return rc;
    }

    int_enable.s_tap_int_enable         = tap_type == BMA253_TAP_TYPE_SINGLE;
    int_enable.d_tap_int_enable         = tap_type == BMA253_TAP_TYPE_DOUBLE;

    rc = bma253_set_int_enable(bma253, &int_enable);
    if (rc != 0) {
        goto done;
    }

    wait_interrupt(&bma253->intr, pdd->int_num);

    rc = bma253_set_int_enable(bma253, &int_enable_org);
    if (rc != 0) {
        goto done;
    }

    rc = default_power(bma253);

done:
    pdd->interrupt = NULL;
    disable_intpin(bma253);
    /* Restore previous routing */
    rc = bma253_set_int_routes(bma253, &int_routes_org);

    return rc;
#else
    return SYS_ENODEV;
#endif
}

int
bma253_power_settings(struct bma253 * bma253,
                      enum bma253_power_mode power_mode,
                      enum bma253_sleep_duration sleep_duration)
{
    struct bma253_cfg * cfg;

    cfg = &bma253->cfg;

    cfg->power_mode     = power_mode;
    cfg->sleep_duration = sleep_duration;

    return default_power(bma253);
}

static int
sensor_driver_read(struct sensor *sensor,
                   sensor_type_t sensor_type,
                   sensor_data_func_t data_func,
                   void *data_arg,
                   uint32_t timeout)
{
    int rc;
    const struct bma253_cfg *cfg;
    struct bma253 *bma253;

    if ((sensor_type & ~(SENSOR_TYPE_ACCELEROMETER |
                         SENSOR_TYPE_AMBIENT_TEMPERATURE)) != 0) {
        rc = SYS_EINVAL;
        goto err;
    }

    bma253 = (struct bma253 *)SENSOR_GET_DEVICE(sensor);
    cfg = &bma253->cfg;

    if (cfg->read_mode == BMA253_READ_M_POLL) {
        rc = bma253_poll_read(sensor, sensor_type, data_func, data_arg, timeout);
    } else {
        rc = bma253_stream_read(sensor, sensor_type, data_func, data_arg, timeout);
    }

    return 0;
err:
    return rc;
}

int
bma253_poll_read(struct sensor * sensor,
                 sensor_type_t sensor_type,
                 sensor_data_func_t data_func,
                 void * data_arg,
                 uint32_t timeout)
{
    struct bma253 *bma253;
    const struct bma253_cfg *cfg;
    enum bma253_power_mode request_power[3];
    int rc;
    struct accel_data accel_data[AXIS_ALL];
    struct sensor_accel_data sad;
    float temp_c;
    struct sensor_temp_data std;

    bma253 = (struct bma253 *)SENSOR_GET_DEVICE(sensor);
    cfg = &bma253->cfg;

    request_power[0] = BMA253_POWER_MODE_LPM_1;
    request_power[1] = BMA253_POWER_MODE_LPM_2;
    request_power[2] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        return rc;
    }

    if ((sensor_type & SENSOR_TYPE_ACCELEROMETER) != 0) {
        rc = bma253_get_accel(bma253,
                              cfg->g_range,
                              AXIS_X,
                              accel_data + AXIS_X);
        if (rc != 0) {
            return rc;
        }
        rc = bma253_get_accel(bma253,
                              cfg->g_range,
                              AXIS_Y,
                              accel_data + AXIS_Y);
        if (rc != 0) {
            return rc;
        }
        rc = bma253_get_accel(bma253,
                              cfg->g_range,
                              AXIS_Z,
                              accel_data + AXIS_Z);
        if (rc != 0) {
            return rc;
        }

        sad.sad_x = accel_data[AXIS_X].accel_g;
        sad.sad_y = accel_data[AXIS_Y].accel_g;
        sad.sad_z = accel_data[AXIS_Z].accel_g;
        sad.sad_x_is_valid = 1;
        sad.sad_y_is_valid = 1;
        sad.sad_z_is_valid = 1;

        rc = data_func(sensor,
                       data_arg,
                       &sad,
                       SENSOR_TYPE_ACCELEROMETER);
        if (rc != 0) {
            return rc;
        }
    }

    if ((sensor_type & SENSOR_TYPE_AMBIENT_TEMPERATURE) != 0) {
        rc = bma253_get_temp(bma253, &temp_c);
        if (rc != 0) {
            return rc;
        }

        std.std_temp = temp_c;
        std.std_temp_is_valid = 1;

        rc = data_func(sensor,
                       data_arg,
                       &std,
                       SENSOR_TYPE_AMBIENT_TEMPERATURE);
        if (rc != 0) {
            return rc;
        }
    }

    rc = default_power(bma253);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
sensor_driver_get_config(struct sensor * sensor,
                         sensor_type_t sensor_type,
                         struct sensor_cfg * cfg)
{
    if ((sensor_type & ~(SENSOR_TYPE_ACCELEROMETER |
                         SENSOR_TYPE_AMBIENT_TEMPERATURE)) != 0) {
        return SYS_EINVAL;
    }
    if ((sensor_type & (sensor_type - 1)) != 0) {
        return SYS_EINVAL;
    }

    if ((sensor_type & SENSOR_TYPE_ACCELEROMETER) != 0) {
        cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;
    }
    if ((sensor_type & SENSOR_TYPE_AMBIENT_TEMPERATURE) != 0) {
        cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;
    }

    return 0;
}

static int
sensor_driver_set_trigger_thresh(struct sensor * sensor,
                                 sensor_type_t sensor_type,
                                 struct sensor_type_traits * stt)
{
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    struct bma253 * bma253;
    const struct bma253_cfg * cfg;
    int rc;
    enum bma253_power_mode request_power[3];
    const struct sensor_accel_data * low_thresh;
    const struct sensor_accel_data * high_thresh;
    struct int_enable int_enable;
    float thresh;
    struct low_g_int_cfg low_g_int_cfg;
    struct high_g_int_cfg high_g_int_cfg;
    struct bma253_private_driver_data *pdd;

    if (sensor_type != SENSOR_TYPE_ACCELEROMETER) {
        return SYS_EINVAL;
    }

    bma253 = (struct bma253 *)SENSOR_GET_DEVICE(sensor);
    cfg = &bma253->cfg;
    pdd = &bma253->pdd;

    pdd->read_ctx.srec_type |= sensor_type;
    pdd->registered_mask |= BMA253_READ_MASK;
    enable_intpin(bma253);

    request_power[0] = BMA253_POWER_MODE_LPM_1;
    request_power[1] = BMA253_POWER_MODE_LPM_2;
    request_power[2] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        goto done;
    }

    low_thresh  = stt->stt_low_thresh.sad;
    high_thresh = stt->stt_high_thresh.sad;

    rc = bma253_get_int_enable(bma253, &int_enable);
    if (rc != 0) {
        goto done;
    }

    if (low_thresh->sad_x_is_valid |
        low_thresh->sad_y_is_valid |
        low_thresh->sad_z_is_valid) {
        thresh = INFINITY;

        if (low_thresh->sad_x_is_valid) {
            if (thresh > low_thresh->sad_x) {
                thresh = low_thresh->sad_x;
            }
        }
        if (low_thresh->sad_y_is_valid) {
            if (thresh > low_thresh->sad_y) {
                thresh = low_thresh->sad_y;
            }
        }
        if (low_thresh->sad_z_is_valid) {
            if (thresh > low_thresh->sad_z) {
                thresh = low_thresh->sad_z;
            }
        }

        low_g_int_cfg.delay_ms     = 20;
        low_g_int_cfg.thresh_g     = thresh;
        low_g_int_cfg.hyster_g     = 0.125;
        low_g_int_cfg.axis_summing = false;

        rc = bma253_set_low_g_int_cfg(bma253,
                                      &low_g_int_cfg);
        if (rc != 0) {
            goto done;
        }

        int_enable.low_g_int_enable = true;
    }

    if (high_thresh->sad_x_is_valid |
        high_thresh->sad_y_is_valid |
        high_thresh->sad_z_is_valid) {
        thresh = 0.0;

        if (high_thresh->sad_x_is_valid) {
            if (thresh < high_thresh->sad_x) {
                thresh = high_thresh->sad_x;
            }
        }
        if (high_thresh->sad_y_is_valid) {
            if (thresh < high_thresh->sad_y) {
                thresh = high_thresh->sad_y;
            }
        }
        if (high_thresh->sad_z_is_valid) {
            if (thresh < high_thresh->sad_z) {
                thresh = high_thresh->sad_z;
            }
        }

        high_g_int_cfg.hyster_g = 0.25;
        high_g_int_cfg.delay_ms = 32;
        high_g_int_cfg.thresh_g = thresh;

        rc = bma253_set_high_g_int_cfg(bma253,
                                       cfg->g_range,
                                       &high_g_int_cfg);
        if (rc != 0) {
            goto done;
        }

        int_enable.high_g_z_int_enable = high_thresh->sad_z_is_valid;
        int_enable.high_g_y_int_enable = high_thresh->sad_y_is_valid;
        int_enable.high_g_x_int_enable = high_thresh->sad_x_is_valid;
    }

    rc = bma253_set_int_enable(bma253, &int_enable);

done:
    if (rc != 0) {
        /* Something went wrong, unregister from interrupt */
        pdd->read_ctx.srec_type &= ~sensor_type;
        pdd->registered_mask &= ~BMA253_READ_MASK;
        disable_intpin(bma253);
    }

    return rc;
#else
    return SYS_ENODEV;
#endif
}

static int
sensor_driver_set_config(struct sensor *sensor, void *cfg)
{
    struct bma253* bma253 = (struct bma253 *)SENSOR_GET_DEVICE(sensor);

    return bma253_config(bma253, (struct bma253_cfg*)cfg);
}

static int
sensor_driver_unset_notification(struct sensor * sensor,
                                 sensor_event_type_t sensor_event_type)
{
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    struct bma253 * bma253;
    enum bma253_power_mode request_power[3];
    struct int_enable int_enable;
    struct int_routes int_routes;
    struct bma253_private_driver_data *pdd;
    int rc;

    if ((sensor_event_type & ~(SENSOR_EVENT_TYPE_DOUBLE_TAP |
                               SENSOR_EVENT_TYPE_SINGLE_TAP)) != 0) {
        return SYS_EINVAL;
    }

    /*XXX for now we do not support registering for both events */
    if (sensor_event_type == (SENSOR_EVENT_TYPE_DOUBLE_TAP |
                                        SENSOR_EVENT_TYPE_SINGLE_TAP)) {
        return SYS_EINVAL;
    }

    bma253 = (struct bma253 *)SENSOR_GET_DEVICE(sensor);
    pdd = &bma253->pdd;

    pdd->notify_ctx.snec_evtype &= ~sensor_event_type;
    pdd->registered_mask &= ~BMA253_NOTIFY_MASK;
    disable_intpin(bma253);

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        return rc;
    }

    /* Clear route and interrupts. We can do it for single and double as driver
     * supports notification only for one of them at the time
     */
    rc = bma253_get_int_routes(bma253, &int_routes);
    if (rc != 0) {
        return rc;
    }

    if (sensor_event_type & SENSOR_EVENT_TYPE_SINGLE_TAP) {
        int_routes.s_tap_int_route = INT_ROUTE_NONE;
    }

    if (sensor_event_type & SENSOR_EVENT_TYPE_DOUBLE_TAP) {
        int_routes.d_tap_int_route = INT_ROUTE_NONE;
    }

    rc = bma253_set_int_routes(bma253, &int_routes);
    if (rc != 0) {
        return rc;
    }

    rc = bma253_get_int_enable(bma253, &int_enable);
    if (rc != 0) {
        return rc;
    }

    int_enable.d_tap_int_enable = false;
    int_enable.s_tap_int_enable = false;

    rc = bma253_set_int_enable(bma253, &int_enable);
    if (rc != 0) {
        return rc;
    }

    return 0;
#else
    return SYS_ENODEV;
#endif
}

static int
sensor_driver_set_notification(struct sensor * sensor,
                               sensor_event_type_t sensor_event_type)
{
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    struct bma253 * bma253;
    int rc;
    enum bma253_power_mode request_power[3];
    struct int_enable int_enable;
    struct int_routes int_routes;
    struct bma253_private_driver_data *pdd;

    if ((sensor_event_type & ~(SENSOR_EVENT_TYPE_DOUBLE_TAP |
                               SENSOR_EVENT_TYPE_SINGLE_TAP)) != 0) {
        return SYS_EINVAL;
    }

    /*XXX for now we do not support registering for both events */
    if (sensor_event_type == (SENSOR_EVENT_TYPE_DOUBLE_TAP |
                                        SENSOR_EVENT_TYPE_SINGLE_TAP)) {
        return SYS_EINVAL;
    }

    bma253 = (struct bma253 *)SENSOR_GET_DEVICE(sensor);
    pdd = &bma253->pdd;

    if (pdd->registered_mask & BMA253_NOTIFY_MASK) {
        return SYS_EBUSY;
    }

    pdd->notify_ctx.snec_evtype |= sensor_event_type;
    pdd->registered_mask |= BMA253_NOTIFY_MASK;
    enable_intpin(bma253);

    request_power[0] = BMA253_POWER_MODE_LPM_1;
    request_power[1] = BMA253_POWER_MODE_LPM_2;
    request_power[2] = BMA253_POWER_MODE_NORMAL;

    rc = interim_power(bma253,
                       request_power,
                       sizeof(request_power) / sizeof(*request_power));
    if (rc != 0) {
        goto done;
    }

    /* Configure route */
    rc = bma253_get_int_routes(bma253, &int_routes);
    if (rc != 0) {
        return rc;
    }

    if (sensor_event_type & SENSOR_EVENT_TYPE_DOUBLE_TAP) {
        int_routes.d_tap_int_route = pdd->int_route;
    }

    if (sensor_event_type & SENSOR_EVENT_TYPE_SINGLE_TAP) {
        int_routes.s_tap_int_route = pdd->int_route;
    }

    rc = bma253_set_int_routes(bma253, &int_routes);
    if (rc != 0) {
        return rc;
    }

    /* Configure enable event*/
    rc = bma253_get_int_enable(bma253, &int_enable);
    if (rc != 0) {
        goto done;
    }

    /* Enable tap event*/
    int_enable.s_tap_int_enable         = sensor_event_type &
                                          SENSOR_EVENT_TYPE_SINGLE_TAP;
    int_enable.d_tap_int_enable         = sensor_event_type &
                                          SENSOR_EVENT_TYPE_DOUBLE_TAP;
    rc = bma253_set_int_enable(bma253, &int_enable);

done:
    if (rc != 0) {
        pdd->notify_ctx.snec_evtype &= ~sensor_event_type;
        pdd->registered_mask &= ~BMA253_NOTIFY_MASK;
        disable_intpin(bma253);
    }

    return rc;
#else
    return SYS_ENODEV;
#endif
}

static int
sensor_driver_handle_interrupt(struct sensor * sensor)
{
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    struct bma253 * bma253;
    struct bma253_private_driver_data *pdd;
    struct int_status int_status;
    int rc;

    bma253 = (struct bma253 *)SENSOR_GET_DEVICE(sensor);
    pdd = &bma253->pdd;

    rc = bma253_get_int_status(bma253, &int_status);
    if (rc != 0) {
        BMA253_ERROR("Cound not read int status err=0x%02x\n", rc);
        return rc;
    }

    if (pdd->registered_mask & BMA253_NOTIFY_MASK) {
        if (int_status.s_tap_int_active) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_SINGLE_TAP);
        }

        if (int_status.d_tap_int_active) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_DOUBLE_TAP);
        }
    }

    if ((pdd->registered_mask & BMA253_READ_MASK) &&
            (int_status.high_g_int_active || int_status.low_g_int_active)) {
        sensor_mgr_put_read_evt(&pdd->read_ctx);
    }

    return 0;
#else
    return SYS_ENODEV;
#endif
}

static struct sensor_driver bma253_sensor_driver = {
    .sd_read               = sensor_driver_read,
    .sd_set_config         = sensor_driver_set_config,
    .sd_get_config         = sensor_driver_get_config,
    .sd_set_trigger_thresh = sensor_driver_set_trigger_thresh,
    .sd_set_notification   = sensor_driver_set_notification,
    .sd_unset_notification = sensor_driver_unset_notification,
    .sd_handle_interrupt   = sensor_driver_handle_interrupt,
};

int
bma253_config(struct bma253 * bma253, struct bma253_cfg * cfg)
{
    struct sensor * sensor;
    int rc;
    uint8_t chip_id;

    bma253->cfg = *cfg;

    sensor = &bma253->sensor;

    rc = bma253_get_chip_id(bma253, &chip_id);
    if (rc != 0) {
        return rc;
    }
    if (chip_id != REG_VALUE_CHIP_ID) {
        BMA253_ERROR("received incorrect chip ID 0x%02X\n", chip_id);
        return SYS_EINVAL;
    }

    rc = reset_and_recfg(bma253);
    if (rc != 0) {
        return rc;
    }

    rc = default_power(bma253);
    if (rc != 0) {
        return rc;
    }

    rc = sensor_set_type_mask(sensor, cfg->sensor_mask);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma253_init(struct os_dev * dev, void * arg)
{
    struct bma253 * bma253;
    struct sensor * sensor;
#if MYNEWT_VAL(BMA253_INT_ENABLE)
    struct bma253_private_driver_data *pdd;
#endif
    int rc;

    if (!dev || !arg) {
        return SYS_ENODEV;
    }

#if MYNEWT_VAL(BMA253_LOG)
    rc = log_register(dev->od_name,
                      &bma253_log,
                      &log_console_handler,
                      NULL,
                      LOG_SYSLEVEL);
    if (rc != 0) {
        return rc;
    }
#endif

    bma253 = (struct bma253 *)dev;
    sensor = &bma253->sensor;

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        return rc;
    }

    rc = sensor_set_driver(sensor,
                           SENSOR_TYPE_ACCELEROMETER |
                           SENSOR_TYPE_AMBIENT_TEMPERATURE,
                           &bma253_sensor_driver);
    if (rc != 0) {
        return rc;
    }

    rc = sensor_set_interface(sensor, arg);
    if (rc != 0) {
        return rc;
    }

    sensor->s_next_run = OS_TIMEOUT_NEVER;

    rc = sensor_mgr_register(sensor);
    if (rc != 0) {
        return rc;
    }

#if MYNEWT_VAL(BMA253_INT_ENABLE)
    init_interrupt(&bma253->intr, bma253->sensor.s_itf.si_ints);

    pdd = &bma253->pdd;

    pdd->read_ctx.srec_sensor = sensor;
    pdd->notify_ctx.snec_sensor = sensor;

    rc = init_intpin(bma253, interrupt_handler, sensor);
    if (rc != 0) {
        return rc;
    }
#endif

    bma253->power = BMA253_POWER_MODE_NORMAL;

    return 0;
}
