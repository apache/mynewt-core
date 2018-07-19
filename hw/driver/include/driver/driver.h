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

#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <string.h>
#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Driver interfaces
 */
#define DRIVER_ITF_SPI    (0)
#define DRIVER_ITF_I2C    (1)
#define DRIVER_ITF_UART   (2)

struct driver_int {
    uint8_t host_pin;
    uint8_t device_pin;
    uint8_t active;
};

struct driver_itf {

    /* Driver interface type */
    uint8_t si_type;

    /* Driver interface number */
    uint8_t si_num;

    /* Driver CS pin */
    uint8_t si_cs_pin;

    /* Driver address */
    uint16_t si_addr;

    /* Driver interface low int pin */
    uint8_t si_low_pin;

    /* Driver interface high int pin */
    uint8_t si_high_pin;

    /* Mutex for interface access */
    struct os_mutex *si_lock;

    /* Driver interface interrupts pins */
    /* XXX We should probably remove low/high pins and replace it with those
     */
    struct driver_int si_ints[MYNEWT_VAL(DRIVER_MAX_INTERRUPTS_PINS)];
};

/**
 * Lock access to the driver_itf specified by si.  Blocks until lock acquired.
 *
 * @param si The driver_itf to lock
 * @param timeout The timeout
 *
 * @return 0 on success, non-zero on failure.
 */
int
driver_itf_lock(struct driver_itf *si, os_time_t timeout);

/**
 * Unlock access to the driver_itf specified by si.
 *
 * @param si The driver_itf to unlock access to
 *
 * @return 0 on success, non-zero on failure.
 */
void
driver_itf_unlock(struct driver_itf *si);


#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_H__ */



