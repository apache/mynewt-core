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

#ifndef __DS1307_H__
#define __DS1307_H__

#include <stats/stats.h>
#include <datetime/datetime.h>

#define DS1307_I2C_ADDR                 0x68

#define DS1307_SECONDS_ADDR             0x00
#define DS1307_MINUTES_ADDR             0x01
#define DS1307_HOURS_ADDR               0x02
#define DS1307_DAY_ADDR                 0x03
#define DS1307_DATE_ADDR                0x04
#define DS1307_MONTH_ADDR               0x05
#define DS1307_YEAR_ADDR                0x06
#define DS1307_CONTROL_ADDR             0x07

#define DS1307_CONTROL_OUT              0x80
#define DS1307_CONTROL_SQWE             0x10
#define DS1307_CONTROL_RS1              0x02
#define DS1307_CONTROL_RS0              0x01

#define DS1307_SW_RATE_1HZ              0
#define DS1307_SW_RATE_4096HZ           1
#define DS1307_SW_RATE_8192HZ           2
#define DS1307_SW_RATE_32768HZ          3

#define DS1307_HOURS_12                 0x40
#define DS1307_HOURS_PM                 0x20

struct ds1307_hw_cfg {
    uint8_t i2c_num;
};

struct ds1307_cfg {
    uint8_t sw_rate : 2;
    uint8_t enable_32khz : 1;
};

/* Define the stats section and records */
STATS_SECT_START(ds1307_stat_section)
    STATS_SECT_ENTRY(read_count)
    STATS_SECT_ENTRY(write_count)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

struct ds1307_dev {
    struct os_dev odev;
    struct ds1307_hw_cfg hw_cfg;
    struct ds1307_cfg cfg;

    STATS_SECT_DECL(ds1307_stat_section) stats;
};

/**
 * Initialize the ds1307. this function is normally called by sysinit.
 *
 * @param dev  Pointer to the ds1307_dev device descriptor
 * @param arg  Pointer to struct ds1307_hw_cfg.
 *
 * @return 0 on success, non-zero on failure.
 */
int ds1307_init(struct os_dev *dev, void *arg);

/**
 * Sets sensor device configuration.
 *
 * @param  ds1307      RTC device
 * @param  ds1307_cfg  Pointer to the ds1307 configuration
 *
 * @return 0 on success, non-zero on failure.
 */
int ds1307_config(struct ds1307_dev *ds1307, const struct ds1307_cfg *cfg);

/**
 * Read clock time from RTC.
 *
 * @param  ds1307      RTC device
 * @param  tm          Read clock time
 *
 * @return 0 on success, non-zero on failure.
 */
int ds1307_read_time(struct ds1307_dev *ds1307, struct clocktime *tm);

/**
 * Write clock time to RTC.
 *
 * @param ds1307    RTC device
 * @param tm        clock time to set RTC to
 *
 * @return 0 on success, non-zero on failure.
 */
int ds1307_write_time(struct ds1307_dev *ds1307, const struct clocktime *tm);

/**
 * Read RTC register(s).
 *
 * @param ds1307    RTC device
 * @param addr      first register to read from
 * @param regs      buffer for read register values
 * @param count     number of register to read
 *
 * @return 0 on success, non-zero on failure.
 */
int ds1307_read_regs(struct ds1307_dev *ds1307, uint8_t addr, uint8_t *regs, uint8_t count);

/**
 * Write RTC register(s).
 *
 * @param ds1307    RTC device
 * @param addr      first register address to write to
 * @param vals      buffer with register values to write
 * @param count     number of registers to write
 *
 * @return 0 on success, non-zero on failure.
 */
int ds1307_write_regs(struct ds1307_dev *ds1307, uint8_t addr, uint8_t *vals, uint8_t count);


#if MYNEWT_VAL(DS1307_CLI)
/**
 * Initialize the DS1307 shell extensions.
 *
 * @return 0 on success, non-zero on failure.
 */
void ds1307_shell_init(void);
#endif


#ifdef __cplusplus
}
#endif

#endif /* __DS1307_H__ */
