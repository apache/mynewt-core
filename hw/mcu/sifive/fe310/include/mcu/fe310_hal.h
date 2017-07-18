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

#ifndef H_FE310_HAL_
#define H_FE310_HAL_

#include <stdint.h>
#include <env/encoding.h>

#ifdef __cplusplus
 extern "C" {
#endif

struct fe310_uart_cfg {
    int8_t suc_pin_tx;                          /* pins for IO */
    int8_t suc_pin_rx;
};

/* Helper functions to enable/disable interrupts. */
#define __HAL_DISABLE_INTERRUPTS(x)                        \
    do {                                                   \
        x = clear_csr(mstatus, MSTATUS_MIE) & MSTATUS_MIE; \
    } while(0);

#define __HAL_ENABLE_INTERRUPTS(x)                         \
    do {                                                   \
        if (x) {                                           \
            set_csr(mstatus, MSTATUS_MIE);                 \
        }                                                  \
    } while(0);

extern const struct hal_flash fe310_flash_dev;

void hal_uart_sys_clock_changed(void);

void hal_uart_sys_clock_changed(void);

#ifdef __cplusplus
}
#endif

#endif  /* H_FE310_HAL_ */
