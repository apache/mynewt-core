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

#ifndef __UART_BITBANG_API_H__
#define __UART_BITBANG_API_H__

int uart_bitbang_config(int port, int32_t baudrate, uint8_t databits,
  uint8_t stopbits, enum hal_uart_parity parity,
  enum hal_uart_flow_ctl flow_ctl);
int uart_bitbang_init_cbs(int port, hal_uart_tx_char tx_func,
  hal_uart_tx_done tx_done, hal_uart_rx_char rx_func, void *arg);
void uart_bitbang_start_rx(int port);
void uart_bitbang_start_tx(int port);
void uart_bitbang_blocking_tx(int port, uint8_t data);
int uart_bitbang_close(int port);

#endif
