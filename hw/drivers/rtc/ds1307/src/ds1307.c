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

#include <ds1307/ds1307.h>

/* Define stat names for querying */
STATS_NAME_START(ds1307_stat_section)
    STATS_NAME(ds1307_stat_section, read_count)
    STATS_NAME(ds1307_stat_section, write_count)
    STATS_NAME(ds1307_stat_section, read_errors)
    STATS_NAME(ds1307_stat_section, write_errors)
STATS_NAME_END(ds1307_stat_section)

int
ds1307_write_regs(struct ds1307_dev *ds1307, uint8_t addr, uint8_t *vals, uint8_t count)
{
    int rc;
    uint8_t payload[count + 1];

    payload[0] = addr;
    memcpy(payload + 1, vals, count);

    struct hal_i2c_master_data data_struct = {
        .address = DS1307_I2C_ADDR,
        .len = count + 1,
        .buffer = payload,
    };

    STATS_INC(ds1307->stats, write_count);
    rc = i2cn_master_write(ds1307->hw_cfg.i2c_num, &data_struct, OS_TICKS_PER_SEC / 10, 1, 1);
    if (rc) {
        STATS_INC(ds1307->stats, write_errors);
        DS1307_LOG_ERROR("DS1307 write I2C failed\n");
    }

    return rc;
}

int
ds1307_read_regs(struct ds1307_dev *ds1307, uint8_t addr, uint8_t *regs, uint8_t count)
{
    int rc;
    uint8_t payload[1] = { addr };

    struct hal_i2c_master_data data_struct = {
        .address = DS1307_I2C_ADDR,
        .len = 1,
        .buffer = payload
    };

    STATS_INC(ds1307->stats, read_count);
    rc = i2cn_master_write(ds1307->hw_cfg.i2c_num, &data_struct, OS_TICKS_PER_SEC / 10, 1, 1);
    if (rc) {
        STATS_INC(ds1307->stats, read_errors);
        DS1307_LOG_ERROR("DS1307 write I2C failed\n");
        goto exit;
    }

    data_struct.len = count;
    data_struct.buffer = regs;
    rc = i2cn_master_read(ds1307->hw_cfg.i2c_num, &data_struct, OS_TICKS_PER_SEC / 10, 1, 1);
    if (rc) {
        STATS_INC(ds1307->stats, read_errors);
        DS1307_LOG_ERROR("DS1307 read I2C failed\n");
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
ds1307_read_time(struct ds1307_dev *ds1307, struct clocktime *tm)
{
    int rc;
    uint8_t buf[0x13];

    rc = ds1307_read_regs(ds1307, DS1307_SECONDS_ADDR, buf, 0x13);
    if (rc == 0) {
        tm->usec = 0;
        /* BCD encoded values */
        tm->sec = bcd_to_bin(buf[0] & 0x7f);
        tm->min = bcd_to_bin(buf[1] & 0x7f);
        if (buf[2] & DS1307_HOURS_12) {
            tm->hour = bcd_to_bin(buf[2] & 0x1f);
            /* AM/PM */
            if (buf[2] & DS1307_HOURS_PM) {
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

static uint8_t
bin_to_bcd(int bin)
{
    return (bin % 10) + ((bin / 10) << 4);
}

int
ds1307_write_time(struct ds1307_dev *ds1307, const struct clocktime *tm)
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

    rc = ds1307_write_regs(ds1307, DS1307_SECONDS_ADDR, buf, 7);

    return rc;
}

int
ds1307_config(struct ds1307_dev *ds1307, const struct ds1307_cfg *cfg)
{
    int rc;
    uint8_t control;

    ds1307_read_regs(ds1307, DS1307_CONTROL_ADDR, &control, 1);
    if (cfg->enable_32khz) {
        control |= DS1307_CONTROL_SQWE;
    } else {
        control &= ~DS1307_CONTROL_SQWE;
    }
    control &= ~(DS1307_CONTROL_RS1 | DS1307_CONTROL_RS0);
    control |= (cfg->sw_rate & 3);

    rc = ds1307_write_regs(ds1307, DS1307_CONTROL_ADDR, &control, 1);

    return rc;
}

static int
ds1307_open_handler(struct os_dev *dev, uint32_t wait, void *arg)
{
    /* Default values after power on. */
    int rc = 0;
    struct ds1307_dev *ds1307 = (struct ds1307_dev *)dev;
    (void)wait;

    if (arg) {
        ds1307->cfg = *(struct ds1307_cfg *)arg;
        rc = ds1307_config(ds1307, (struct ds1307_cfg *)arg);
    }

    return rc;
}

static int
ds1307_close_handler(struct os_dev *dev)
{
    (void)dev;

    return 0;
}

int
ds1307_init(struct os_dev *dev, void *arg)
{
    struct ds1307_dev *ds1307;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto exit;
    }

    ds1307 = (struct ds1307_dev *)dev;
    ds1307->hw_cfg = *(struct ds1307_hw_cfg *)arg;
    ds1307->cfg.enable_32khz = 0;
    ds1307->cfg.sw_rate = 0;

    /* Initialise the stats entry. */
    rc = stats_init(
        STATS_HDR(ds1307->stats),
        STATS_SIZE_INIT_PARMS(ds1307->stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(ds1307_stat_section));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);
    /* Register the entry with the stats registry */
    rc = stats_register(ds1307->odev.od_name, STATS_HDR(ds1307->stats));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);

    OS_DEV_SETHANDLERS(dev, ds1307_open_handler, ds1307_close_handler);
exit:
    return rc;
}

void
ds1307_create(struct ds1307_dev *dev, const char *name, struct ds1307_hw_cfg *cfg)
{
    int rc;
    rc = os_dev_create((struct os_dev *)dev, name,
                       OS_DEV_INIT_PRIMARY, 0, ds1307_init, cfg);
    assert(rc == 0);
}

struct ds1307_dev *
ds1307_open(const char *name, struct ds1307_cfg *cfg)
{
    return (struct ds1307_dev *)os_dev_open(name, 0, cfg);
}

static struct ds1307_dev ds1307;
static struct ds1307_hw_cfg hw_cfg = {
    .i2c_num = MYNEWT_VAL(DS1307_I2C_NUM),
};

void
ds1307_pkg_init(void)
{
    struct ds1307_dev *rtc;
    struct clocktime ct;
    struct os_timeval tv;
    struct os_timezone tz;

    ds1307_create(&ds1307, MYNEWT_VAL(DS1307_OS_DEV_NAME), &hw_cfg);

    rtc = ds1307_open(MYNEWT_VAL(DS1307_OS_DEV_NAME), NULL);
    if (rtc) {
        if (ds1307_read_time(rtc, &ct) == 0) {
            os_gettimeofday(&tv, &tz);
            clocktime_to_timeval(&ct, &tz, &tv);
            os_settimeofday(&tv, &tz);
        }
        os_dev_close((struct os_dev *)rtc);
    }
}
