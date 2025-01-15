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

#ifndef _STM32_PIN_CFG_H
#define _STM32_PIN_CFG_H

#include <stdint.h>
#include <mcu/stm32_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

struct stm32_pin_cfg {
    int8_t pin;
    GPIO_InitTypeDef hal_init;
};

typedef const struct stm32_pin_cfg *stm32_pin_cfg_t;

#ifdef __cplusplus
}
#endif

#endif /* _STM32_PIN_CFG_H */
