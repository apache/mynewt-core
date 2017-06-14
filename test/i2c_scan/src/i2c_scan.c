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
#include <os/os.h>

#include <hal/hal_i2c.h>
#include <shell/shell.h>
#include <console/console.h>
#include <parse/parse.h>

static int i2c_scan_cli_cmd(int argc, char **argv);

static struct shell_cmd i2c_scan_cmd_struct = {
    .sc_cmd = "i2c_scan",
    .sc_cmd_func = i2c_scan_cli_cmd
};

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
#ifndef ARCH_sim
        rc = hal_i2c_master_probe(i2cnum, addr, timeout);
#else
        (void)timeout;
#endif
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

void
i2c_scan_init(void)
{
    shell_cmd_register(&i2c_scan_cmd_struct);
}
