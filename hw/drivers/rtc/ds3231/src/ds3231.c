/*
 * Copyright 2022 Jesus Ipanienko
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <assert.h>

#include <os/mynewt.h>
#include <hal/hal_i2c.h>
#include <i2cn/i2cn.h>
#include <modlog/modlog.h>
#include <stats/stats.h>

#include <ds3231/ds3231.h>

/* Define stat names for querying */
STATS_NAME_START(ds3231_stat_section)
    STATS_NAME(ds3231_stat_section, read_count)
    STATS_NAME(ds3231_stat_section, write_count)
    STATS_NAME(ds3231_stat_section, read_errors)
    STATS_NAME(ds3231_stat_section, write_errors)
STATS_NAME_END(ds3231_stat_section)

int
ds3231_write_regs(struct ds3231_dev *ds3231, uint8_t addr, uint8_t *vals, uint8_t count)
{
    int rc;
    uint8_t payload[count + 1];

    payload[0] = addr;
    memcpy(payload + 1, vals, count);

    struct hal_i2c_master_data data_struct = {
        .address = ds3231->hw_cfg.i2c_addr,
        .len = count + 1,
        .buffer = payload,
    };

    STATS_INC(ds3231->stats, write_count);
    rc = i2cn_master_write(ds3231->hw_cfg.i2c_num, &data_struct, OS_TICKS_PER_SEC / 10, 1, 1);
    if (rc) {
        STATS_INC(ds3231->stats, write_errors);
        DS3231_LOG_ERROR("DS3231 write I2C failed\n");
    }

    return rc;
}

int
ds3231_read_regs(struct ds3231_dev *ds3231, uint8_t addr, uint8_t *regs, uint8_t count)
{
    int rc;
    uint8_t payload[1] = { addr };

    struct hal_i2c_master_data data_struct = {
        .address = ds3231->hw_cfg.i2c_addr,
        .len = 1,
        .buffer = payload
    };

    STATS_INC(ds3231->stats, read_count);
    rc = i2cn_master_write(ds3231->hw_cfg.i2c_num, &data_struct, OS_TICKS_PER_SEC / 10, 1, 1);
    if (rc) {
        STATS_INC(ds3231->stats, read_errors);
        DS3231_LOG_ERROR("DS3231 write I2C failed\n");
        goto exit;
    }

    data_struct.len = count;
    data_struct.buffer = regs;
    rc = i2cn_master_read(ds3231->hw_cfg.i2c_num, &data_struct, OS_TICKS_PER_SEC / 10, 1, 1);
    if (rc) {
        STATS_INC(ds3231->stats, read_errors);
        DS3231_LOG_ERROR("DS3231 read I2C failed\n");
    }

exit:
    return rc;
}

static int
bcd_to_bin(uint8_t bcd)
{
    return (bcd & 0xf) + (bcd >> 4) * 10;
}

int
ds3231_read_time(struct ds3231_dev *ds3231, struct clocktime *tm)
{
    int rc;
    uint8_t buf[0x13];

    rc = ds3231_read_regs(ds3231, DS3231_SECONDS_ADDR, buf, 0x13);
    if (rc == 0) {
        tm->usec = 0;
        /* BCD encoded values */
        tm->sec = bcd_to_bin(buf[0] & 0x7f);
        tm->min = bcd_to_bin(buf[1] & 0x7f);
        if (buf[2] & DS3231_HOURS_12) {
            tm->hour = bcd_to_bin(buf[2] & 0x1f);
            /* AM/PM */
            if (buf[2] & DS3231_HOURS_PM) {
                tm->hour += 12;
            }
        } else {
            tm->hour = bcd_to_bin(buf[2] & 0x3f);
        }
        tm->dow = buf[3] - 1;
        tm->day = bcd_to_bin(buf[4]);
        tm->mon = bcd_to_bin(buf[5] & 0x1f);
        tm->year = 2000 + bcd_to_bin(buf[6]);
    }

    return rc;
}

int
ds3231_read_temp(struct ds3231_dev *ds3231, int16_t *temperature)
{
    int rc;
    uint8_t buf[2];

    rc = ds3231_read_regs(ds3231, DS3231_TEMP_MSB_ADDR, buf, 2);
    if (rc == 0) {
        *temperature = (((buf[0] << 8) | buf[1]) >> 6) * 25;
    }

    return rc;
}

static uint8_t
bin_to_bcd(int bin)
{
    return (bin % 10) + ((bin / 10) << 4);
}

int
ds3231_write_time(struct ds3231_dev *ds3231, const struct clocktime *tm)
{
    int rc;
    uint8_t buf[7];

    /* Encode to BCD */
    buf[0] = bin_to_bcd(tm->sec);
    buf[1] = bin_to_bcd(tm->min);
    buf[2] = bin_to_bcd(tm->hour);
    buf[3] = tm->day + 1;
    buf[4] = bin_to_bcd(tm->day);
    buf[5] = bin_to_bcd(tm->mon);
    buf[6] = bin_to_bcd(tm->year % 100);

    rc = ds3231_write_regs(ds3231, DS3231_SECONDS_ADDR, buf, 7);

    return rc;
}

int
ds3231_config(struct ds3231_dev *ds3231, const struct ds3231_cfg *cfg)
{
    int rc;
    uint8_t regs[2];

    ds3231_read_regs(ds3231, DS3231_CONTROL_ADDR, regs, 2);
    if (cfg->enable_32khz) {
        regs[1] |= DS3231_CONTROL_STATUS_EN32KHZ;
    } else {
        regs[1] &= ~DS3231_CONTROL_STATUS_EN32KHZ;
    }
    if (cfg->bbsqw) {
        regs[0] |= DS3231_CONTROL_BBSQW;
    } else {
        regs[0] &= ~DS3231_CONTROL_BBSQW;
    }
    regs[0] &= ~(DS3231_CONTROL_RS1 | DS3231_CONTROL_RS2);
    regs[0] |= (cfg->sw_rate << 3);

    rc = ds3231_write_regs(ds3231, DS3231_CONTROL_ADDR, regs, 2);

    return rc;
}

static int
ds3231_open_handler(struct os_dev *dev, uint32_t wait, void *arg)
{
    /* Default values after power on. */
    int rc = 0;
    struct ds3231_dev *ds3231 = (struct ds3231_dev *)dev;
    (void)wait;

    if (arg) {
        ds3231->cfg = *(struct ds3231_cfg *)arg;
        rc = ds3231_config(ds3231, (struct ds3231_cfg *)arg);
    }

    return rc;
}

static int
ds3231_close_handler(struct os_dev *dev)
{
    (void)dev;

    return 0;
}

int
ds3231_init(struct os_dev *dev, void *arg)
{
    struct ds3231_dev *ds3231;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto exit;
    }

    ds3231 = (struct ds3231_dev *)dev;
    ds3231->hw_cfg = *(struct ds3231_hw_cfg *)arg;
    ds3231->cfg.bbsqw = 0;
    ds3231->cfg.enable_32khz = 0;
    ds3231->cfg.sw_rate = 0;

    /* Initialise the stats entry. */
    rc = stats_init(
        STATS_HDR(ds3231->stats),
        STATS_SIZE_INIT_PARMS(ds3231->stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(ds3231_stat_section));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);
    /* Register the entry with the stats registry */
    rc = stats_register(ds3231->odev.od_name, STATS_HDR(ds3231->stats));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);

    OS_DEV_SETHANDLERS(dev, ds3231_open_handler, ds3231_close_handler);
exit:
    return rc;
}

void
ds3231_create(struct ds3231_dev *dev, const char *name, struct ds3231_hw_cfg *cfg)
{
    int rc;
    rc = os_dev_create((struct os_dev *)dev, name,
                       OS_DEV_INIT_PRIMARY, 0, ds3231_init, cfg);
    assert(rc == 0);
}

struct ds3231_dev *
ds3231_open(const char *name, struct ds3231_cfg *cfg)
{
    return (struct ds3231_dev *)os_dev_open(name, 0, cfg);
}

struct ds3231_dev ds3231;
struct ds3231_hw_cfg hw_cfg = {
    .i2c_num = MYNEWT_VAL(DS3231_I2C_NUM),
    .i2c_addr = 0x68,
    .int_pin = -1,
    .sqt_pin = -1,
};

void
ds3231_pkg_init(void)
{
    struct ds3231_dev *rtc;
    struct clocktime ct;
    struct os_timeval tv;
    struct os_timezone tz;

    ds3231_create(&ds3231, MYNEWT_VAL(DS3231_OS_DEV_NAME), &hw_cfg);

    rtc = ds3231_open(MYNEWT_VAL(DS3231_OS_DEV_NAME), NULL);
    if (rtc) {
        if (ds3231_read_time(rtc, &ct) == 0) {
            os_gettimeofday(&tv, &tz);
            clocktime_to_timeval(&ct, &tz, &tv);
            os_settimeofday(&tv, &tz);
        }
        os_dev_close((struct os_dev *)rtc);
    }
}
