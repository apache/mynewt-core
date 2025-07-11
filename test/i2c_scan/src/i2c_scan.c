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

#include <inttypes.h>
#include "os/mynewt.h"
#include <hal/hal_i2c.h>
#include <shell/shell.h>
#include <console/console.h>
#include <parse/parse.h>
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include <bus/drivers/i2c_common.h>
#endif

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static int
i2c_scan_probe(int i2c_num, uint16_t address, uint32_t timeout)
{
    char bus_name[5] = "i2cX";
    struct bus_i2c_dev *bus;
    int rc = SYS_EINVAL;

    bus_name[3] = i2c_num + '0';

    bus = (struct bus_i2c_dev *)os_dev_open(bus_name, timeout, NULL);
    if (bus) {
        rc = bus_i2c_probe(bus, address, timeout);
        (void)os_dev_close((struct os_dev *)bus);
    }

    return rc;
}
#else
static int
i2c_scan_probe(int i2cnum, uint16_t address, uint32_t timeout)
{
    return hal_i2c_master_probe(i2cnum, address, timeout);
}
#endif

static int
i2c_scan_cli_cmd(int argc, char **argv)
{
    uint8_t addr;
    int32_t timeout;
    uint8_t dev_count = 0;
    uint8_t i2cnum;
    int rc;

    if (argc < 2) {
        console_printf("Specify i2c num\n");
        return 0;
    }

    timeout = OS_TICKS_PER_SEC / 10;

    rc = 0;
    i2cnum = parse_ll_bounds(argv[1], 0, 0xf, &rc);
    if (rc) {
        console_printf("Invalid i2c interface:%s\n", argv[2]);
        return 0;
    }

    console_printf("Scanning I2C bus %u\n"
                   "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n"
                   "00:                         ", i2cnum);

    /* Scan all valid I2C addresses (0x08..0x77) */
    for (addr = 0x08; addr < 0x78; addr++) {
        rc = i2c_scan_probe(i2cnum, addr, timeout);
        /* Print addr header every 16 bytes */
        if (!(addr % 16)) {
            console_printf("\n%02x: ", addr);
        }
        /* Display the addr if a response was received */
        if (!rc) {
            console_printf("%02x ", addr);
            dev_count++;
        } else {
            console_printf("-- ");
        }
        os_time_delay(OS_TICKS_PER_SEC / 1000 * 20);
    }
    console_printf("\nFound %u devices on I2C bus %u\n", dev_count, i2cnum);

    return 0;
}

MAKE_SHELL_CMD(i2c_scan, i2c_scan_cli_cmd, NULL)
