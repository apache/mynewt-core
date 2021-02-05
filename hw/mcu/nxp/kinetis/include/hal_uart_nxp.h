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

#ifndef __MCU_FRDMK64F_HAL_UART_H_
#define __MCU_FRDMK64F_HAL_UART_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define NXP_UART_EXISTS      { 1, \
                               1, \
                               (FSL_FEATURE_SOC_UART_COUNT > 2U), \
                               (FSL_FEATURE_SOC_UART_COUNT > 3U), \
                               (FSL_FEATURE_SOC_UART_COUNT > 4U), \
                               (FSL_FEATURE_SOC_UART_COUNT > 5U) }

#define NXP_UART_ENABLED     { MYNEWT_VAL(UART_0), \
                               MYNEWT_VAL(UART_1), \
                               MYNEWT_VAL(UART_2), \
                               MYNEWT_VAL(UART_3), \
                               MYNEWT_VAL(UART_4), \
                               MYNEWT_VAL(UART_5) }

#define NXP_UART_CLOCKS      { UART0_CLK_SRC, \
                               UART1_CLK_SRC, \
                               UART2_CLK_SRC, \
                               UART3_CLK_SRC, \
                               UART4_CLK_SRC, \
                               UART5_CLK_SRC }

#define NXP_UART_PORTS       { MYNEWT_VAL(UART_0_PORT), \
                               MYNEWT_VAL(UART_1_PORT), \
                               MYNEWT_VAL(UART_2_PORT), \
                               MYNEWT_VAL(UART_3_PORT), \
                               MYNEWT_VAL(UART_4_PORT), \
                               MYNEWT_VAL(UART_5_PORT) }

#define NXP_UART_PORT_CLOCKS { MYNEWT_VAL(UART_0_PORT_CLOCK), \
                               MYNEWT_VAL(UART_1_PORT_CLOCK), \
                               MYNEWT_VAL(UART_2_PORT_CLOCK), \
                               MYNEWT_VAL(UART_3_PORT_CLOCK), \
                               MYNEWT_VAL(UART_4_PORT_CLOCK), \
                               MYNEWT_VAL(UART_5_PORT_CLOCK) }

#define NXP_UART_PIN_RX      { MYNEWT_VAL(UART_0_PIN_RX), \
                               MYNEWT_VAL(UART_1_PIN_RX), \
                               MYNEWT_VAL(UART_2_PIN_RX), \
                               MYNEWT_VAL(UART_3_PIN_RX), \
                               MYNEWT_VAL(UART_4_PIN_RX), \
                               MYNEWT_VAL(UART_5_PIN_RX) }

#define NXP_UART_PIN_TX      { MYNEWT_VAL(UART_0_PIN_TX), \
                               MYNEWT_VAL(UART_1_PIN_TX), \
                               MYNEWT_VAL(UART_2_PIN_TX), \
                               MYNEWT_VAL(UART_3_PIN_TX), \
                               MYNEWT_VAL(UART_4_PIN_TX), \
                               MYNEWT_VAL(UART_5_PIN_TX) }

#ifdef __cplusplus
}
#endif

#endif /* __MCU_FRDMK64F_HAL_UART_H_ */
