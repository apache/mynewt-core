/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "hal/hal_gpio.h"
#include "mcu/stm32f30x.h"
#include "mcu/stm32f30x_rcc.h"
#include "mcu/stm32f30x_gpio.h"
#include "mcu/stm32f3_bsp.h"
#include "bsp/bsp.h"

static const struct stm32f3_uart_cfg uart_cfg[UART_CNT] = {
    [0] = {
        .suc_uart = USART1,
        .suc_rcc_cmd = RCC_APB2PeriphClockCmd,
        .suc_rcc_dev = RCC_APB2Periph_USART1,
        .suc_pin_tx = 9,
        .suc_pin_rx = 10,
        .suc_pin_rts = 12,
        .suc_pin_cts = 11,
        .suc_pin_af = GPIO_AF_7,
        .suc_irqn = USART1_IRQn
    }
};

const struct stm32f3_uart_cfg *bsp_uart_config(int port)
{
    assert(port < UART_CNT);
    return &uart_cfg[port];
}
