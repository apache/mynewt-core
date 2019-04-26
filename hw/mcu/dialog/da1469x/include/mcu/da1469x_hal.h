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

#ifndef __MCU_DA1469X_HAL_H_
#define __MCU_DA1469X_HAL_H_

#include <assert.h>
#include "hal/hal_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Helper functions to enable/disable interrupts. */
#define __HAL_DISABLE_INTERRUPTS(x)                     \
    do {                                                \
        x = __get_PRIMASK();                            \
        __disable_irq();                                \
    } while (0)

#define __HAL_ENABLE_INTERRUPTS(x)                      \
    do {                                                \
        if (!x) {                                       \
            __enable_irq();                             \
        }                                               \
    } while (0)

#define __HAL_ASSERT_CRITICAL()                         \
    do {                                                \
        assert(__get_PRIMASK() & 1);                    \
    } while (0)

extern const struct hal_flash da1469x_flash_dev;

struct da1469x_uart_cfg {
    int8_t pin_tx;
    int8_t pin_rx;
    int8_t pin_rts;
    int8_t pin_cts;
};

struct da1469x_hal_i2c_cfg {
    int8_t pin_scl;
    int8_t pin_sda;
    uint32_t frequency;
};

struct da1469x_hal_spi_cfg {
    int8_t pin_sck;
    int8_t pin_do;
    int8_t pin_di;
    int8_t pin_ss;
};

#ifdef __cplusplus
}
#endif

#endif  /* __MCU_DA1469X_HAL_H_ */
