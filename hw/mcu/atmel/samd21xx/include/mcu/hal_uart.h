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

#ifndef __HAL_UART_H__
#define __HAL_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

struct samd21_uart_config {
    Sercom *suc_sercom;
    enum usart_signal_mux_settings suc_mux_setting;
    enum gclk_generator suc_generator_source;
    enum usart_sample_rate suc_sample_rate;
    enum usart_sample_adjustment suc_sample_adjustment;
    uint32_t suc_pad0;
    uint32_t suc_pad1;
    uint32_t suc_pad2;
    uint32_t suc_pad3;
};

/*
 * This fetches UART config for this UART port.
*/
const struct samd21_uart_config *bsp_uart_config(int port);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */

