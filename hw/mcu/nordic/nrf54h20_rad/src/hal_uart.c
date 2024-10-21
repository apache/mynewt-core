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

// #include <assert.h>
// #include <os/mynewt.h>
// #include <hal/hal_uart.h>
// #include <mcu/cmsis_nvic.h>
// #include <bsp/bsp.h>
// #include <nrf.h>
// #include <mcu/nrf54h20_rad_hal.h>

// #if MYNEWT_VAL(UART_0)
// #endif

// #define UARTE_INT_ENDTX         UARTE_INTEN_ENDTX_Msk
// #define UARTE_INT_ENDRX         UARTE_INTEN_ENDRX_Msk
// #define UARTE_CONFIG_PARITY     UARTE_CONFIG_PARITY_Msk
// #define UARTE_CONFIG_PARITY_ODD UARTE_CONFIG_PARITYTYPE_Msk
// #define UARTE_CONFIG_HWFC       UARTE_CONFIG_HWFC_Msk
// #define UARTE_ENABLE            UARTE_ENABLE_ENABLE_Enabled
// #define UARTE_DISABLE           UARTE_ENABLE_ENABLE_Disabled

// struct hal_uart {
//     uint8_t u_open : 1;
//     uint8_t u_rx_stall : 1;
//     volatile uint8_t u_tx_started : 1;

//     uint8_t u_rx_buf;
//     uint8_t u_tx_buf[8];

//     hal_uart_rx_char u_rx_func;
//     hal_uart_tx_char u_tx_func;
//     hal_uart_tx_done u_tx_done;
//     void *u_func_arg;
// };

// static struct hal_uart uart0;

// int
// hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
//                   hal_uart_rx_char rx_func, void *arg)
// {
//     if (port != 0 || uart0.u_open) {
//         return -1;
//     }

//     uart0.u_rx_func = rx_func;
//     uart0.u_tx_func = tx_func;
//     uart0.u_tx_done = tx_done;
//     uart0.u_func_arg = arg;

//     return 0;
// }

// static int
// hal_uart_tx_fill_buf(struct hal_uart *u)
// {
//     int data;
//     int i;

//     for (i = 0; i < sizeof(u->u_tx_buf); i++) {
//         data = u->u_tx_func(u->u_func_arg);
//         if (data < 0) {
//             break;
//         }
//         u->u_tx_buf[i] = data;
//     }
//     return i;
// }

// void
// hal_uart_start_tx(int port)
// {
//     int sr;
//     int rc;

//     if (port != 0) {
//         return;
//     }

//     __HAL_DISABLE_INTERRUPTS(sr);
//     if (uart0.u_tx_started == 0) {
//         rc = hal_uart_tx_fill_buf(&uart0);
//         if (rc > 0) {
//             NRF_UARTE0_NS->INTENSET = UARTE_INT_ENDTX;
//             NRF_UARTE0_NS->TXD.PTR = (uint32_t)&uart0.u_tx_buf;
//             NRF_UARTE0_NS->TXD.MAXCNT = rc;
//             NRF_UARTE0_NS->TASKS_STARTTX = 1;
//             uart0.u_tx_started = 1;
//         }
//     }
//     __HAL_ENABLE_INTERRUPTS(sr);
// }

// void
// hal_uart_start_rx(int port)
// {
//     int sr;
//     int rc;

//     if (port != 0) {
//         return;
//     }

//     if (uart0.u_rx_stall) {
//         __HAL_DISABLE_INTERRUPTS(sr);
//         rc = uart0.u_rx_func(uart0.u_func_arg, uart0.u_rx_buf);
//         if (rc == 0) {
//             uart0.u_rx_stall = 0;
//             NRF_UARTE0_NS->TASKS_STARTRX = 1;
//         }

//         __HAL_ENABLE_INTERRUPTS(sr);
//     }
// }

// void
// hal_uart_blocking_tx(int port, uint8_t data)
// {
//     if (port != 0 || !uart0.u_open) {
//         return;
//     }

//     /* If we have started, wait until the current uart dma buffer is done */
//     if (uart0.u_tx_started) {
//         while (NRF_UARTE0_NS->EVENTS_ENDTX == 0) {
//             /* Wait here until the dma is finished */
//         }
//     }

//     NRF_UARTE0_NS->EVENTS_ENDTX = 0;
//     NRF_UARTE0_NS->TXD.PTR = (uint32_t)&data;
//     NRF_UARTE0_NS->TXD.MAXCNT = 1;
//     NRF_UARTE0_NS->TASKS_STARTTX = 1;

//     while (NRF_UARTE0_NS->EVENTS_ENDTX == 0) {
//         /* Wait till done */
//     }

//     /* Stop the uart */
//     NRF_UARTE0_NS->TASKS_STOPTX = 1;
// }

// static void
// uart0_irq_handler(void)
// {
//     int rc;

//     os_trace_isr_enter();

//     if (NRF_UARTE0_NS->EVENTS_ENDTX) {
//         NRF_UARTE0_NS->EVENTS_ENDTX = 0;
//         rc = hal_uart_tx_fill_buf(&uart0);
//         if (rc > 0) {
//             NRF_UARTE0_NS->TXD.PTR = (uint32_t)uart0.u_tx_buf;
//             NRF_UARTE0_NS->TXD.MAXCNT = rc;
//             NRF_UARTE0_NS->TASKS_STARTTX = 1;
//         } else {
//             if (uart0.u_tx_done) {
//                 uart0.u_tx_done(uart0.u_func_arg);
//             }
//             NRF_UARTE0_NS->INTENCLR = UARTE_INT_ENDTX;
//             NRF_UARTE0_NS->TASKS_STOPTX = 1;
//             uart0.u_tx_started = 0;
//         }
//     }
//     if (NRF_UARTE0_NS->EVENTS_ENDRX) {
//         NRF_UARTE0_NS->EVENTS_ENDRX = 0;
//         rc = uart0.u_rx_func(uart0.u_func_arg, uart0.u_rx_buf);
//         if (rc < 0) {
//             uart0.u_rx_stall = 1;
//         } else {
//             NRF_UARTE0_NS->TASKS_STARTRX = 1;
//         }
//     }
//     os_trace_isr_exit();
// }

// static uint32_t
// hal_uart_baudrate(int baudrate)
// {
//     switch (baudrate) {
//     case 1200:
//         return UARTE_BAUDRATE_BAUDRATE_Baud1200;
//     case 2400:
//         return UARTE_BAUDRATE_BAUDRATE_Baud2400;
//     case 4800:
//         return UARTE_BAUDRATE_BAUDRATE_Baud4800;
//     case 9600:
//         return UARTE_BAUDRATE_BAUDRATE_Baud9600;
//     case 14400:
//         return UARTE_BAUDRATE_BAUDRATE_Baud14400;
//     case 19200:
//         return UARTE_BAUDRATE_BAUDRATE_Baud19200;
//     case 28800:
//         return UARTE_BAUDRATE_BAUDRATE_Baud28800;
//     case 38400:
//         return UARTE_BAUDRATE_BAUDRATE_Baud38400;
//     case 56000:
//         return UARTE_BAUDRATE_BAUDRATE_Baud56000;
//     case 57600:
//         return UARTE_BAUDRATE_BAUDRATE_Baud57600;
//     case 76800:
//         return UARTE_BAUDRATE_BAUDRATE_Baud76800;
//     case 115200:
//         return UARTE_BAUDRATE_BAUDRATE_Baud115200;
//     case 230400:
//         return UARTE_BAUDRATE_BAUDRATE_Baud230400;
//     case 250000:
//         return UARTE_BAUDRATE_BAUDRATE_Baud250000;
//     case 460800:
//         return UARTE_BAUDRATE_BAUDRATE_Baud460800;
//     case 921600:
//         return UARTE_BAUDRATE_BAUDRATE_Baud921600;
//     case 1000000:
//         return UARTE_BAUDRATE_BAUDRATE_Baud1M;
//     default:
//         return 0;
//     }
// }

// int
// hal_uart_init(int port, void *arg)
// {
//     struct nrf54h20_rad_uart_cfg *cfg = arg;

//     if (port != 0) {
//         return -1;
//     }

//     NRF_UARTE0_NS->PSEL.TXD = cfg->suc_pin_tx;
//     NRF_UARTE0_NS->PSEL.RXD = cfg->suc_pin_rx;
//     NRF_UARTE0_NS->PSEL.RTS = cfg->suc_pin_rts;
//     NRF_UARTE0_NS->PSEL.CTS = cfg->suc_pin_cts;

//     NVIC_SetVector(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn,
//                    (uint32_t)uart0_irq_handler);

//     return 0;
// }

// int
// hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
//                 enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
// {
//     uint32_t cfg_reg = 0;
//     uint32_t baud_reg;

//     if (port != 0 || uart0.u_open) {
//         return -1;
//     }

//     /*
//      * pin config
//      * UART config
//      * nvic config
//      * enable uart
//      */
//     if (databits != 8) {
//         return -1;
//     }
//     if (stopbits != 1) {
//         return -1;
//     }

//     switch (parity) {
//     case HAL_UART_PARITY_NONE:
//         break;
//     case HAL_UART_PARITY_ODD:
//         cfg_reg |= UARTE_CONFIG_PARITY | UARTE_CONFIG_PARITY_ODD;
//         break;
//     case HAL_UART_PARITY_EVEN:
//         cfg_reg |= UARTE_CONFIG_PARITY;
//         break;
//     }

//     switch (flow_ctl) {
//     case HAL_UART_FLOW_CTL_NONE:
//         break;
//     case HAL_UART_FLOW_CTL_RTS_CTS:
//         cfg_reg |= UARTE_CONFIG_HWFC;
//         if (NRF_UARTE0_NS->PSEL.RTS == 0xffffffff ||
//             NRF_UARTE0_NS->PSEL.CTS == 0xffffffff) {
//             /*
//              * Can't turn on HW flow control if pins to do that are not
//              * defined.
//              */
//             assert(0);
//             return -1;
//         }
//         break;
//     }
//     baud_reg = hal_uart_baudrate(baudrate);
//     if (baud_reg == 0) {
//         return -1;
//     }
//     NRF_UARTE0_NS->ENABLE = 0;
//     NRF_UARTE0_NS->INTENCLR = 0xffffffff;
//     NRF_UARTE0_NS->BAUDRATE = baud_reg;
//     NRF_UARTE0_NS->CONFIG = cfg_reg;

//     NVIC_EnableIRQ(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn);

//     NRF_UARTE0_NS->ENABLE = UARTE_ENABLE;

//     NRF_UARTE0_NS->INTENSET = UARTE_INT_ENDRX;
//     NRF_UARTE0_NS->RXD.PTR = (uint32_t)&uart0.u_rx_buf;
//     NRF_UARTE0_NS->RXD.MAXCNT = sizeof(uart0.u_rx_buf);
//     NRF_UARTE0_NS->TASKS_STARTRX = 1;

//     uart0.u_rx_stall = 0;
//     uart0.u_tx_started = 0;
//     uart0.u_open = 1;

//     return 0;
// }

// int
// hal_uart_close(int port)
// {
//     if (port != 0) {
//         return -1;
//     }

//     uart0.u_open = 0;
//     while (uart0.u_tx_started) {
//         /* Wait here until the dma is finished */
//     }
//     NRF_UARTE0_NS->ENABLE = 0;
//     NRF_UARTE0_NS->INTENCLR = 0xffffffff;

//     return 0;
// }
