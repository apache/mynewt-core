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

#ifndef _STM32_CLOCK_H
#define _STM32_CLOCK_H

#include <mcu/stm32_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef const struct stm32_clock *stm32_clock_t;
typedef const struct stm32_clock_ops *stm32_clock_ops_t;
typedef const struct stm32_clock_src *stm32_clock_src_t;

struct stm32_clock_ops {
    void (*enable)(stm32_clock_t clock);
    void (*disable)(stm32_clock_t clock);
};

struct stm32_clock {
    stm32_clock_ops_t ops;
};

void stm32_clock_enable(stm32_clock_t clock);
void stm32_clock_disable(stm32_clock_t clock);

#ifndef STM32_PER_CLOCK_DEF
#define STM32_PER_CLOCK_DEF(per) \
extern const struct stm32_clock _##per##_clock;\
static const struct stm32_clock *const per##_clock = &_##per##_clock;

#define PER_CLOCK(per) per##_clock

#define STM32_CLKSEL_DEF1(per, rcc_reg, reg_field, reg_val)\
extern const struct stm32_clock_src _##per##_##reg_val;\
static stm32_clock_t const per##_##reg_val __unused = (stm32_clock_t)&_##per##_##reg_val;

#define STM32_CLKSEL_DEF2(per, rcc_reg, reg_field, reg_val)\
extern const struct stm32_clock_src _##per##_##reg_val;\
static stm32_clock_t const per##_##reg_val __unused = (stm32_clock_t)&_##per##_##reg_val;

#include <mcu/per_clocks.h>
#endif

#ifdef __cplusplus
}
#endif

#endif /* _STM32_CLOCK_H */
