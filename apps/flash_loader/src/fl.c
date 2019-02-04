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
#include <sys/types.h>
#include <inttypes.h>
#include <string.h>

#include <os/mynewt.h>

#include <bsp/bsp.h>
#include <hal/hal_bsp.h>
#include <hal/hal_flash.h>
#include <hal/hal_gpio.h>
#include <hal/hal_watchdog.h>

#include "flash_loader/flash_loader.h"

/*
 * Anything marked as 'volatile' is either expected to be read and/or written
 * by programmer.
 */
volatile int fl_state;
volatile int fl_cmd;
volatile int fl_cmd_rc;

uint8_t *fl_data;
volatile uint8_t *fl_cmd_data;
volatile uint32_t fl_cmd_amount;
volatile uint32_t fl_cmd_flash_id;
volatile uint32_t fl_cmd_flash_addr;
int fl_cmd_data_sz = MYNEWT_VAL(FLASH_LOADER_DL_SZ) / 2;

/*
 * Load/verify use doble buffering scheme. Programmer can write the
 * data for next flash operation while app is executing previous command.
 */
struct {
    uint8_t *buf;
    uint32_t amount;
    uint32_t flash_id;
    uint32_t addr;
} fl_write;

uint8_t fl_verify_buf[MYNEWT_VAL(FLASH_LOADER_VERIFY_BUF_SZ)];

static void
fl_rotate_databuf(void)
{
    fl_write.buf = (uint8_t *)fl_cmd_data;
    fl_write.amount = fl_cmd_amount;
    fl_write.flash_id = fl_cmd_flash_id;
    fl_write.addr = fl_cmd_flash_addr;

    if (fl_cmd_data == fl_data) {
        fl_cmd_data = &fl_data[fl_cmd_data_sz];
    } else {
        fl_cmd_data = fl_data;
    }
    fl_cmd_amount = 0;
}

static int
fl_load_cmd(void)
{
    int rc;

    if (fl_write.amount > fl_cmd_data_sz) {
        return FL_RC_ARG_ERR;
    }
    rc = hal_flash_write(fl_write.flash_id, fl_write.addr,
                         fl_write.buf, fl_write.amount);
    if (rc) {
        return FL_RC_FLASH_ERR;
    }
    return FL_RC_OK;
}

static int
fl_erase_cmd(void)
{
    int rc;

    rc = hal_flash_erase(fl_cmd_flash_id, fl_cmd_flash_addr, fl_cmd_amount);
    if (rc) {
        return FL_RC_FLASH_ERR;
    }
    return FL_RC_OK;
}

static int
fl_verify_cmd(void)
{
    int rc;
    uint32_t off;
    int blen;

    if (fl_write.amount > fl_cmd_data_sz) {
        return FL_RC_ARG_ERR;
    }
    for (off = 0; off < fl_write.amount; off += blen) {
        blen = fl_write.amount - off;
        if (blen > sizeof(fl_verify_buf)) {
            blen = sizeof(fl_verify_buf);
        }
        rc = hal_flash_read(fl_write.flash_id, fl_write.addr + off,
                            fl_verify_buf, blen);
        if (rc) {
            return FL_RC_FLASH_ERR;
        }
        if (memcmp(fl_verify_buf, fl_write.buf + off, blen)) {
            return FL_RC_VERIFY_ERR;
        }
    }
    return FL_RC_OK;
}

static int
fl_dump_cmd(void)
{
    int rc;

    rc = hal_flash_read(fl_cmd_flash_id, fl_cmd_flash_addr, (void *)fl_cmd_data,
                        fl_cmd_amount);
    if (rc) {
        return FL_RC_FLASH_ERR;
    }
    return FL_RC_OK;
}

/*
 * Blinks led if running (and LED is defined).
 */
static void
blink_led(void)
{
#if LED_BLINK_PIN
    static int init = 0;
    static int fl_loop_cntr;

    if (!init) {
        hal_gpio_init_out(LED_BLINK_PIN, 0);
        init = 1;
    }
    if (fl_loop_cntr > MYNEWT_VAL(FLASH_LOADER_LOOP_PER_BLINK)) {
        fl_loop_cntr = 0;
        hal_gpio_toggle(LED_BLINK_PIN);
    } else {
        fl_loop_cntr++;
    }
#endif
}

int
main(int argc, char **argv)
{
    int rc;

    hal_bsp_init();
    flash_map_init();
    hal_watchdog_init(MYNEWT_VAL(WATCHDOG_INTERVAL));
    hal_watchdog_enable();

    fl_data = malloc(MYNEWT_VAL(FLASH_LOADER_DL_SZ));
    assert(fl_data);
    fl_cmd_data  = fl_data;
    while (1) {
        if (!fl_cmd) {
            fl_state = FL_WAITING;
            blink_led();
            continue;
        }
        fl_state = FL_EXECUTING;
        switch (fl_cmd) {
        case FL_CMD_PING:
            fl_cmd = 0;
            rc = FL_RC_OK;
            break;
        case FL_CMD_LOAD:
            fl_rotate_databuf();
            fl_cmd = 0;
            rc = fl_load_cmd();
            break;
        case FL_CMD_ERASE:
            fl_cmd = 0;
            rc = fl_erase_cmd();
            break;
        case FL_CMD_VERIFY:
            fl_rotate_databuf();
            fl_cmd = 0;
            rc = fl_verify_cmd();
            break;
        case FL_CMD_LOAD_VERIFY:
            fl_rotate_databuf();
            fl_cmd = 0;
            rc = fl_load_cmd();
            if (rc == FL_RC_OK) {
                rc = fl_verify_cmd();
            }
            break;
        case FL_CMD_DUMP:
            fl_cmd = 0;
            rc = fl_dump_cmd();
            break;
        default:
            fl_cmd = 0;
            rc = FL_RC_UNKNOWN_CMD_ERR;
            break;
        }
        hal_watchdog_tickle();
        if (fl_cmd_rc <= FL_RC_OK) {
            fl_cmd_rc = rc;
        }
    }
}
