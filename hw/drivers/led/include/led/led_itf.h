/**
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

#ifndef __LED_ITF_H__
#define __LED_ITF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "syscfg/syscfg.h"

/**
 * LED interfaces
 */
#define LED_ITF_SPI    (0)
#define LED_ITF_I2C    (1)
#define LED_ITF_UART   (2)

/*
 * LED interface
 */
struct led_itf {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    /* Device */
    struct os_dev *li_dev;
#else
    /* LED interface type */
    uint8_t li_type;

    /* interface number */
    uint8_t li_num;

    /* CS pin - optional, only needed for SPI */
    uint8_t li_cs_pin;

    /* LED chip address, only needed for I2C */
    uint16_t li_addr;

    /* Mutex for shared interface access */
    struct os_mutex *li_lock;
#endif
};

/**
 * Lock access to the led_itf specified by li.  Blocks until lock acquired.
 *
 * @param li The led_itf to lock
 * @param timeout The timeout in milliseconds
 *
 * @return 0 on success, non-zero on failure.
 */
static inline int
led_itf_lock(struct led_itf *li, uint32_t timeout)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    return 0;
#else
    int rc;
    os_time_t ticks;

    if (!li->li_lock) {
        return 0;
    }

    rc = os_time_ms_to_ticks(timeout, &ticks);
    if (rc) {
        return rc;
    }

    rc = os_mutex_pend(li->li_lock, ticks);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }

    return (rc);
#endif
}

/**
 * Unlock access to the led_itf specified by li.
 *
 * @param li The led_itf to unlock access to
 *
 * @return 0 on success, non-zero on failure.
 */
static inline void
led_itf_unlock(struct led_itf *li)
{
#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    if (!li->li_lock) {
        return;
    }

    os_mutex_release(li->li_lock);
#endif
}

#endif
