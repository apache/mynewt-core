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

#ifndef __DS3231_H__
#define __DS3231_H__

#include <stats/stats.h>
#include <datetime/datetime.h>

#define DS3231_SECONDS_ADDR             0x00
#define DS3231_MINUTES_ADDR             0x01
#define DS3231_HOURS_ADDR               0x02
#define DS3231_DAY_ADDR                 0x03
#define DS3231_DATE_ADDR                0x04
#define DS3231_MONTH_ADDR               0x05
#define DS3231_YEAR_ADDR                0x06
#define DS3231_ALARM1_SECONS_ADDR       0x07
#define DS3231_ALARM1_MINUTES_ADDR      0x08
#define DS3231_ALARM1_HOURS_ADDR        0x09
#define DS3231_ALARM1_DAY_ADDR          0x0A
#define DS3231_ALARM2_MINUTES_ADDR      0x0B
#define DS3231_ALARM2_HOURS_ADDR        0x0C
#define DS3231_ALARM2_DAY_ADDR          0x0D
#define DS3231_CONTROL_ADDR             0x0E
#define DS3231_CONTROL_STATUS_ADDR      0x0F
#define DS3231_AGING_OFFSET_ADDR        0x10
#define DS3231_TEMP_MSB_ADDR            0x11
#define DS3231_TEMP_LSB_ADDR            0x12

#define DS3231_CONTROL_EOSC             0x80
#define DS3231_CONTROL_BBSQW            0x40
#define DS3231_CONTROL_CONV             0x20
#define DS3231_CONTROL_RS2              0x10
#define DS3231_CONTROL_RS1              0x08
#define DS3231_CONTROL_INTCN            0x04
#define DS3231_CONTROL_A2IE             0x02
#define DS3231_CONTROL_A1IE             0x01

#define DS3231_CONTROL_STATUS_OSF       0x80
#define DS3231_CONTROL_STATUS_EN32KHZ   0x08
#define DS3231_CONTROL_STATUS_BSY       0x04
#define DS3231_CONTROL_STATUS_A2F       0x02
#define DS3231_CONTROL_STATUS_A1F       0x01

#define DS3231_SW_RATE_1HZ              0
#define DS3231_SW_RATE_1024HZ           1
#define DS3231_SW_RATE_4096HZ           2
#define DS3231_SW_RATE_8192HZ           3

#define DS3231_HOURS_12                 0x40
#define DS3231_HOURS_PM                 0x20

struct ds3231_hw_cfg {
    uint8_t i2c_num;
    uint8_t i2c_addr;
    int16_t int_pin;
    int16_t sqt_pin;
};

struct ds3231_cfg {
    uint8_t bbsqw : 1;
    uint8_t sw_rate : 2;
    uint8_t enable_32khz : 1;
};

/* Define the stats section and records */
STATS_SECT_START(ds3231_stat_section)
    STATS_SECT_ENTRY(read_count)
    STATS_SECT_ENTRY(write_count)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

struct ds3231_dev {
    struct os_dev odev;
    struct ds3231_hw_cfg hw_cfg;
    struct ds3231_cfg cfg;

    STATS_SECT_DECL(ds3231_stat_section) stats;
};

/**
 * Initialize the ds3231. this function is normally called by sysinit.
 *
 * @param  ds3231      RTC device
 * @param arg          Pointer to struct ds3231_hw_cfg.
 *
 * @return 0 on success, non-zero on failure.
 */
int ds3231_init(struct os_dev *dev, void *arg);

/**
 * Sets RTC device configuration.
 *
 * @param  ds3231      RTC device
 * @param  ds3231_cfg  Pointer to the ds3231 configuration
 *
 * @return 0 on success, non-zero on failure.
 */
int ds3231_config(struct ds3231_dev *ds3231, const struct ds3231_cfg *cfg);

/**
 * Read clock time from RTC.
 *
 * @param  ds3231      RTC device
 * @param  tm          Read clock time
 *
 * @return 0 on success, non-zero on failure.
 */
int ds3231_read_time(struct ds3231_dev *ds3231, struct clocktime *tm);

/**
 * Read RTC temperature.
 *
 * @param  ds3231      RTC device
 * @param  temperature Read temperature in 0.01 deg C
 *
 * @return 0 on success, non-zero on failure.
 */
int ds3231_read_temp(struct ds3231_dev *ds3231, int16_t *temperature);

/**
 * Write clock time to RTC.
 *
 * @param ds3231    RTC device
 * @param tm        clock time to set RTC to
 *
 * @return 0 on success, non-zero on failure.
 */
int ds3231_write_time(struct ds3231_dev *ds3231, const struct clocktime *tm);

/**
 * Read RTC register(s).
 *
 * @param ds3231    RTC device
 * @param addr      first register to read from
 * @param regs      buffer for read register values
 * @param count     number of register to read
 *
 * @return 0 on success, non-zero on failure.
 */
int ds3231_read_regs(struct ds3231_dev *ds3231, uint8_t addr, uint8_t *regs, uint8_t count);

/**
 * Write RTC register(s).
 *
 * @param ds3231    RTC device
 * @param addr      first register address to write to
 * @param vals      buffer with register values to write
 * @param count     number of registers to write
 *
 * @return 0 on success, non-zero on failure.
 */
int ds3231_write_regs(struct ds3231_dev *ds3231, uint8_t addr, uint8_t *vals, uint8_t count);

#ifdef __cplusplus
}
#endif

#endif /* __DS3231_H__ */
