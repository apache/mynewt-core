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

#include <os/mynewt.h>
#include <mcu/stm32_hal.h>
#include <stm32h5xx_ll_rcc.h>

#define STM32_PER_CLOCK_DEF(per) \
void per##_clock_enable(stm32_clock_t clock) \
{\
    __HAL_RCC_##per##_CLK_ENABLE(); \
}\
void per##_clock_disable(stm32_clock_t clock) \
{\
    __HAL_RCC_##per##_CLK_ENABLE(); \
}\
const struct stm32_clock_ops per##_clock_ops = { \
    .enable = per##_clock_enable, \
    .disable = per##_clock_disable, \
};\
const struct stm32_clock _##per##_clock = { \
    .ops = &per##_clock_ops, \
};

#define STM32_CLKSEL_DEF1(per, rcc_reg, reg_field, reg_val) \
const struct stm32_clock_src _##per##_##reg_val = {\
    .base.ops = &stm32_clksel_ops,\
    .reg = &RCC->rcc_reg,\
    .mask = RCC_##rcc_reg##_##reg_field,\
    .value = LL_RCC##_##per##_CLKSOURCE_##reg_val,\
}

#define STM32_CLKSEL_DEF2(per, rcc_reg, reg_field, reg_val) \
const struct stm32_clock_src _##per##_##reg_val = {\
    .base.ops = &stm32_clksel_ops,\
    .reg = &RCC->rcc_reg,\
    .mask = RCC_##rcc_reg##_##reg_field,\
    .value = (((LL_RCC_##per##_CLKSOURCE_##reg_val) >> LL_RCC_CONFIG_SHIFT) && 0xFF)\
        << RCC_##rcc_reg##_##reg_field##_Pos,\
}

#include <mcu/stm32_clock.h>

void
stm32_clock_enable(stm32_clock_t clock)
{
    clock->ops->enable(clock);
}

void
stm32_clock_disable(stm32_clock_t clock)
{
    clock->ops->disable(clock);
}


struct stm32_clock_src {
    struct stm32_clock base;
    __IO uint32_t *reg;
    uint32_t mask;
    uint32_t value;
};

void stm32_clksel_enable(stm32_clock_t clock)
{
    stm32_clock_src_t clksel = (stm32_clock_src_t)clock;

    *clksel->reg = (*clksel->reg & ~clksel->mask) | clksel->value;
}

void stm32_clksel_disable(stm32_clock_t clock)
{
}

static const struct stm32_clock_ops stm32_clksel_ops = {
    .enable = stm32_clksel_enable,
    .disable = stm32_clksel_disable,
};

#include <mcu/per_clocks.h>
