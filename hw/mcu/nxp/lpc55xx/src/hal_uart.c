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

#include <assert.h>
#include <stdlib.h>

#include <os/mynewt.h>
#include <hal/hal_uart.h>
#include <hal/hal_gpio.h>
#include <hal/hal_system.h>
#include <mcu/cmsis_nvic.h>
#include <mcu/mcux_hal.h>
#include <bsp/bsp.h>

#include <fsl_usart.h>
#include <fsl_iocon.h>

#define IOCON_PIN(uart_num, pin, func) (((func) << 8) | ((uart_num) << 5) | pin)
#define P0_0_FC3_SCK                IOCON_PIN(0, 0, 2)
#define P0_1_FC3_CTS_SDAX_SSEL0     IOCON_PIN(0, 1, 2)
#define P0_2_FC3_TXD_SCL_MISO       IOCON_PIN(0, 2, 1)
#define P0_3_FC3_RXD_SDA_MOSI       IOCON_PIN(0, 3, 1)
#define P0_4_FC4_SCK                IOCON_PIN(0, 4, 2)
#define P0_4_FC3_CTS_SDAX_SSEL0     IOCON_PIN(0, 4, 8)
#define P0_5_FC4_RXD_SDA_MOSI       IOCON_PIN(0, 5, 2)
#define P0_5_FC3_RTS_SCLX_SSEL1     IOCON_PIN(0, 5, 8)
#define P0_6_FC3_SCK                IOCON_PIN(0, 6, 1)
#define P0_7_FC3_RTS_SCLX_SSEL1     IOCON_PIN(0, 7, 1)
#define P0_7_FC5_SCK                IOCON_PIN(0, 7, 3)
#define P0_7_FC1_SCK                IOCON_PIN(0, 7, 4)
#define P0_8_FC3_SSEL3              IOCON_PIN(0, 8, 1)
#define P0_8_FC5_RXD_SDA_MOSI       IOCON_PIN(0, 8, 3)
#define P0_9_FC3_SSEL2              IOCON_PIN(0, 9, 1)
#define P0_9_FC5_TXD_SCL_MISO       IOCON_PIN(0, 9, 3)
#define P0_10_FC6_SCK               IOCON_PIN(0, 10, 1)
#define P0_10_FC1_TXD_SCL_MISO      IOCON_PIN(0, 10, 4)
#define P0_11_FC6_RXD_SDA_MOSI      IOCON_PIN(0, 11, 1)
#define P0_12_FC3_TXD_SCL_MISO      IOCON_PIN(0, 12, 1)
#define P0_12_FC6_TXD_SCL_MISO      IOCON_PIN(0, 12, 7)
#define P0_13_FC1_CTS_SDAX_SSEL0    IOCON_PIN(0, 13, 1)
#define P0_13_FC1_RXD_SDA_MOSI      IOCON_PIN(0, 13, 5)
#define P0_14_FC1_RTS_SCLX_SSEL1    IOCON_PIN(0, 14, 1)
#define P0_14_FC1_TXD_SCL_MISO      IOCON_PIN(0, 14, 6)
#define P0_15_FC6_CTS_SDAX_SSEL0    IOCON_PIN(0, 15, 1)
#define P0_16_FC4_TXD_SCL_MISO      IOCON_PIN(0, 16, 1)
#define P0_17_FC4_SSEL2             IOCON_PIN(0, 17, 1)
#define P0_18_FC4_CTS_SDAX_SSEL0    IOCON_PIN(0, 18, 1)
#define P0_19_FC1_RTS_SCLX_SSEL1    IOCON_PIN(0, 19, 1)
#define P0_19_FC7_TXD_SCL_MISO      IOCON_PIN(0, 19, 7)
#define P0_20_FC3_CTS_SDAX_SSEL0    IOCON_PIN(0, 20, 1)
#define P0_20_FC7_RXD_SDA_MOSI      IOCON_PIN(0, 20, 7)
#define P0_20_FC4_TXD_SCL_MISO      IOCON_PIN(0, 20, 11)
#define P0_21_FC3_RTS_SCLX_SSEL1    IOCON_PIN(0, 21, 1)
#define P0_21_FC7_SCK               IOCON_PIN(0, 21, 7)
#define P0_22_FC6_TXD_SCL_MISO      IOCON_PIN(0, 22, 1)
#define P0_23_FC0_CTS_SDAX_SSEL0    IOCON_PIN(0, 23, 5)
#define P0_24_FC0_RXD_SDA_MOSI      IOCON_PIN(0, 24, 1)
#define P0_25_FC0_TXD_SCL_MISO      IOCON_PIN(0, 25, 1)
#define P0_26_FC2_RXD_SDA_MOSI      IOCON_PIN(0, 26, 1)
#define P0_26_FC0_SCK               IOCON_PIN(0, 26, 8)
#define P0_27_FC2_TXD_SCL_MISO      IOCON_PIN(0, 27, 1)
#define P0_27_FC7_RXD_SDA_MOSI      IOCON_PIN(0, 27, 7)
#define P0_28_FC0_SCK               IOCON_PIN(0, 28, 1)
#define P0_29_FC0_RXD_SDA_MOSI      IOCON_PIN(0, 29, 1)
#define P0_30_FC0_TXD_SCL_MISO      IOCON_PIN(0, 30, 1)
#define P0_31_FC0_CTS_SDAX_SSEL0    IOCON_PIN(0, 31, 1)
#define P1_0_FC0_RTS_SCLX_SSEL1     IOCON_PIN(1, 0, 1)
#define P1_1_FC3_RXD_SDA_MOSI       IOCON_PIN(1, 1, 1)
#define P1_4_FC0_SCK                IOCON_PIN(1, 4, 1)
#define P1_5_FC0_RXD_SDA_MOSI       IOCON_PIN(1, 5, 1)
#define P1_6_FC0_TXD_SCL_MISO       IOCON_PIN(1, 6, 1)
#define P1_7_FC0_RXD_SDA_MOSI       IOCON_PIN(1, 7, 1)
#define P1_8_FC0_CTS_SDAX_SSEL0     IOCON_PIN(1, 8, 1)
#define P1_8_FC4_SSEL2              IOCON_PIN(1, 8, 5)
#define P1_9_FC1_SCK                IOCON_PIN(1, 9, 2)
#define P1_9_FC4_CTS_SDAX_SSEL0     IOCON_PIN(1, 9, 5)
#define P1_10_FC1_RXD_SDA_MOSI      IOCON_PIN(1, 10, 2)
#define P1_11_FC1_TXD_SCL_MISO      IOCON_PIN(1, 11, 2)
#define P1_12_FC6_SCK               IOCON_PIN(1, 12, 2)
#define P1_13_FC6_RXD_SDA_MOSI      IOCON_PIN(1, 13, 2)
#define P1_14_FC5_CTS_SDAX_SSEL0    IOCON_PIN(1, 14, 4)
#define P1_15_FC5_RTS_SCLX_SSEL1    IOCON_PIN(1, 15, 4)
#define P1_15_FC4_RTS_SCLX_SSEL1    IOCON_PIN(1, 15, 5)
#define P1_16_FC6_TXD_SCL_MISO      IOCON_PIN(1, 16, 2)
#define P1_17_FC6_RTS_SCLX_SSEL1    IOCON_PIN(1, 17, 3)
#define P1_19_FC4_SCK               IOCON_PIN(1, 19, 5)
#define P1_20_FC7_RTS_SCLX_SSEL1    IOCON_PIN(1, 20, 1)
#define P1_20_FC4_TXD_SCL_MISO      IOCON_PIN(1, 20, 5)
#define P1_21_FC7_CTS_SDAX_SSEL0    IOCON_PIN(1, 21, 1)
#define P1_21_FC4_RXD_SDA_MOSI      IOCON_PIN(1, 21, 5)
#define P1_22_FC4_SSEL3             IOCON_PIN(1, 22, 5)
#define P1_23_FC2_SCK               IOCON_PIN(1, 23, 1)
#define P1_23_FC3_SSEL2             IOCON_PIN(1, 23, 5)
#define P1_24_FC2_RXD_SDA_MOSI      IOCON_PIN(1, 24, 1)
#define P1_24_FC3_SSEL3             IOCON_PIN(1, 24, 5)
#define P1_25_FC2_TXD_SCL_MISO      IOCON_PIN(1, 25, 1)
#define P1_26_FC2_CTS_SDAX_SSEL0    IOCON_PIN(1, 26, 1)
#define P1_27_FC2_RTS_SCLX_SSEL1    IOCON_PIN(1, 27, 1)
#define P1_28_FC7_SCK               IOCON_PIN(1, 28, 1)
#define P1_29_FC7_RXD_SDA_MOSI      IOCON_PIN(1, 29, 1)
#define P1_28_FC7_TXD_SCL_MISO      IOCON_PIN(1, 30, 1)
#define SPI3_SCK_P0_0               P0_0_FC3_SCK
#define SPI3_SSEL0_P0_1             IOCON_PIN(0, 1, 2)
#define SPI3_SSEL0_P0_1             IOCON_PIN(0, 1, 2)

/*! @brief Ring buffer size (Unit: Byte). */
#define TX_BUF_SZ  32

struct uart_ring {
    uint16_t ur_head;
    uint16_t ur_tail;
    uint16_t ur_size;
    uint8_t _pad;
    uint8_t *ur_buf;
};

struct hal_uart {
    USART_Type *base;
    clock_attach_id_t clk_src;
    uint32_t irqn;
    clock_ip_name_t p_clock;
    int pin_rx;
    int pin_tx;

    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;

    uint8_t u_configured : 1;
    uint8_t u_open : 1;
    uint8_t u_tx_started : 1;
    uint8_t u_rx_stall : 1;

    uint8_t u_rx_buf;

    struct uart_ring ur_tx;
    uint8_t tx_buffer[TX_BUF_SZ];

    usart_handle_t fsl_handle;
    void (*isr)(void);
};

#define UART_INSTANCE(n) \
    static void uart_irq ## n(void);                        \
    static struct hal_uart uart ## n = {                    \
        .base = USART ## n,                                 \
        .clk_src = MYNEWT_VAL_UART_ ## n ## _CLK_SOURCE,    \
        .irqn = FLEXCOMM ## n ## _IRQn,                     \
        .p_clock = kCLOCK_MinUart ## n,                     \
        .pin_rx = MYNEWT_VAL_UART_ ## n ## _PIN_RX,         \
        .pin_tx = MYNEWT_VAL_UART_ ## n ## _PIN_TX,         \
        .ur_tx.ur_buf = uart ## n.tx_buffer,                \
        .ur_tx.ur_size = TX_BUF_SZ,                         \
        .isr = uart_irq ## n,                               \
    };                                                      \
    static void uart_irq ## n(void)                         \
    {                                                       \
        USART_TransferHandleIRQ(uart ## n.base, &uart ## n.fsl_handle); \
    }

UART_INSTANCE(0)
UART_INSTANCE(1)
UART_INSTANCE(2)
UART_INSTANCE(3)
UART_INSTANCE(4)
UART_INSTANCE(5)
UART_INSTANCE(6)
UART_INSTANCE(7)

#define UART_REF(n) (MYNEWT_VAL_UART_ ## n) ? &uart ## n : NULL

static struct hal_uart *uarts[FSL_FEATURE_SOC_FLEXCOMM_COUNT] = {
    UART_REF(0),
    UART_REF(1),
    UART_REF(2),
    UART_REF(3),
    UART_REF(4),
    UART_REF(5),
    UART_REF(6),
    UART_REF(7),
};

typedef enum {
    IOCON_PIN_FUNC_NONE,
    IOCON_PIN_FUNC_CLKOUT,
    IOCON_PIN_FUNC_CMP0_OUT,
    IOCON_PIN_FUNC_CT0MAT0,
    IOCON_PIN_FUNC_CT0MAT1,
    IOCON_PIN_FUNC_CT0MAT2,
    IOCON_PIN_FUNC_CT0MAT3,
    IOCON_PIN_FUNC_CT1MAT0,
    IOCON_PIN_FUNC_CT1MAT1,
    IOCON_PIN_FUNC_CT1MAT2,
    IOCON_PIN_FUNC_CT1MAT3,
    IOCON_PIN_FUNC_CT2MAT0,
    IOCON_PIN_FUNC_CT2MAT1,
    IOCON_PIN_FUNC_CT2MAT2,
    IOCON_PIN_FUNC_CT2MAT3,
    IOCON_PIN_FUNC_CT3MAT0,
    IOCON_PIN_FUNC_CT3MAT1,
    IOCON_PIN_FUNC_CT3MAT2,
    IOCON_PIN_FUNC_CT3MAT3,
    IOCON_PIN_FUNC_CT4MAT0,
    IOCON_PIN_FUNC_CT_INP0,
    IOCON_PIN_FUNC_CT_INP1,
    IOCON_PIN_FUNC_CT_INP2,
    IOCON_PIN_FUNC_CT_INP3,
    IOCON_PIN_FUNC_CT_INP4,
    IOCON_PIN_FUNC_CT_INP5,
    IOCON_PIN_FUNC_CT_INP6,
    IOCON_PIN_FUNC_CT_INP7,
    IOCON_PIN_FUNC_CT_INP8,
    IOCON_PIN_FUNC_CT_INP9,
    IOCON_PIN_FUNC_CT_INP10,
    IOCON_PIN_FUNC_CT_INP12,
    IOCON_PIN_FUNC_CT_INP13,
    IOCON_PIN_FUNC_CT_INP14,
    IOCON_PIN_FUNC_CT_INP15,
    IOCON_PIN_FUNC_CT_INP16,
    IOCON_PIN_FUNC_FC0_CTS_SDAX_SSEL0,
    IOCON_PIN_FUNC_FC0_RTS_SCLX_SSEL1,
    IOCON_PIN_FUNC_FC0_RXD_SDA_MOSI,
    IOCON_PIN_FUNC_FC0_SCK,
    IOCON_PIN_FUNC_FC0_TXD_SCL_MISO,
    IOCON_PIN_FUNC_FC1_CTS_SDAX_SSEL0,
    IOCON_PIN_FUNC_FC1_RTS_SCLX_SSEL1,
    IOCON_PIN_FUNC_FC1_RXD_SDA_MOSI,
    IOCON_PIN_FUNC_FC1_SCK,
    IOCON_PIN_FUNC_FC1_TXD_SCL_MISO,
    IOCON_PIN_FUNC_FC2_CTS_SDAX_SSEL0,
    IOCON_PIN_FUNC_FC2_RTS_SCLX_SSEL1,
    IOCON_PIN_FUNC_FC2_RXD_SDA_MOSI,
    IOCON_PIN_FUNC_FC2_SCK,
    IOCON_PIN_FUNC_FC2_TXD_SCL_MISO,
    IOCON_PIN_FUNC_FC3_CTS_SDAX_SSEL0,
    IOCON_PIN_FUNC_FC3_RTS_SCLX_SSEL1,
    IOCON_PIN_FUNC_FC3_RXD_SDA_MOSI,
    IOCON_PIN_FUNC_FC3_SCK,
    IOCON_PIN_FUNC_FC3_SSEL2,
    IOCON_PIN_FUNC_FC3_SSEL3,
    IOCON_PIN_FUNC_FC3_TXD_SCL_MISO,
    IOCON_PIN_FUNC_FC4_CTS_SDAX_SSEL0,
    IOCON_PIN_FUNC_FC4_RTS_SCLX_SSEL1,
    IOCON_PIN_FUNC_FC4_RXD_SDA_MOSI,
    IOCON_PIN_FUNC_FC4_SCK,
    IOCON_PIN_FUNC_FC4_SSEL2,
    IOCON_PIN_FUNC_FC4_SSEL3,
    IOCON_PIN_FUNC_FC4_TXD_SCL_MISO,
    IOCON_PIN_FUNC_FC5_CTS_SDAX_SSEL0,
    IOCON_PIN_FUNC_FC5_RTS_SCLX_SSEL1,
    IOCON_PIN_FUNC_FC5_RXD_SDA_MOSI,
    IOCON_PIN_FUNC_FC5_SCK,
    IOCON_PIN_FUNC_FC5_TXD_SCL_MISO,
    IOCON_PIN_FUNC_FC6_CTS_SDAX_SSEL0,
    IOCON_PIN_FUNC_FC6_RTS_SCLX_SSEL1,
    IOCON_PIN_FUNC_FC6_RXD_SDA_MOSI,
    IOCON_PIN_FUNC_FC6_SCK,
    IOCON_PIN_FUNC_FC6_TXD_SCL_MISO,
    IOCON_PIN_FUNC_FC7_CTS_SDAX_SSEL0,
    IOCON_PIN_FUNC_FC7_RTS_SCLX_SSEL1,
    IOCON_PIN_FUNC_FC7_RXD_SDA_MOSI,
    IOCON_PIN_FUNC_FC7_SCK,
    IOCON_PIN_FUNC_FC7_TXD_SCL_MISO,
    IOCON_PIN_FUNC_FREQME_GPIO_CLK_A,
    IOCON_PIN_FUNC_FREQME_GPIO_CLK_B,
    IOCON_PIN_FUNC_HS_SPI_SCK,
    IOCON_PIN_FUNC_HS_SPI_SSEL0,
    IOCON_PIN_FUNC_HS_SPI_SSEL1,
    IOCON_PIN_FUNC_HS_SPI_SSEL2,
    IOCON_PIN_FUNC_HS_SPI_SSEL3,
    IOCON_PIN_FUNC_HS_SPI_MISO,
    IOCON_PIN_FUNC_MCLK,
    IOCON_PIN_FUNC_P0_SEC0,
    IOCON_PIN_FUNC_P0_SEC1,
    IOCON_PIN_FUNC_P0_SEC2,
    IOCON_PIN_FUNC_P0_SEC3,
    IOCON_PIN_FUNC_P0_SEC4,
    IOCON_PIN_FUNC_P0_SEC5,
    IOCON_PIN_FUNC_P0_SEC6,
    IOCON_PIN_FUNC_P0_SEC7,
    IOCON_PIN_FUNC_P0_SEC8,
    IOCON_PIN_FUNC_P0_SEC9,
    IOCON_PIN_FUNC_P0_SEC10,
    IOCON_PIN_FUNC_P0_SEC11,
    IOCON_PIN_FUNC_P0_SEC12,
    IOCON_PIN_FUNC_P0_SEC13,
    IOCON_PIN_FUNC_P0_SEC14,
    IOCON_PIN_FUNC_P0_SEC15,
    IOCON_PIN_FUNC_P0_SEC16,
    IOCON_PIN_FUNC_P0_SEC17,
    IOCON_PIN_FUNC_P0_SEC18,
    IOCON_PIN_FUNC_P0_SEC19,
    IOCON_PIN_FUNC_P0_SEC20,
    IOCON_PIN_FUNC_P0_SEC21,
    IOCON_PIN_FUNC_P0_SEC22,
    IOCON_PIN_FUNC_P0_SEC23,
    IOCON_PIN_FUNC_P0_SEC24,
    IOCON_PIN_FUNC_P0_SEC25,
    IOCON_PIN_FUNC_P0_SEC26,
    IOCON_PIN_FUNC_P0_SEC27,
    IOCON_PIN_FUNC_P0_SEC28,
    IOCON_PIN_FUNC_P0_SEC29,
    IOCON_PIN_FUNC_P0_SEC30,
    IOCON_PIN_FUNC_P0_SEC31,
    IOCON_PIN_FUNC_PLU_IN0,
    IOCON_PIN_FUNC_PLU_IN1,
    IOCON_PIN_FUNC_PLU_IN2,
    IOCON_PIN_FUNC_PLU_IN3,
    IOCON_PIN_FUNC_PLU_IN4,
    IOCON_PIN_FUNC_PLU_IN5,
    IOCON_PIN_FUNC_PLU_OUT0,
    IOCON_PIN_FUNC_PLU_OUT1,
    IOCON_PIN_FUNC_PLU_OUT2,
    IOCON_PIN_FUNC_PLU_OUT3,
    IOCON_PIN_FUNC_PLU_OUT4,
    IOCON_PIN_FUNC_PLU_OUT5,
    IOCON_PIN_FUNC_PLU_OUT6,
    IOCON_PIN_FUNC_PLU_OUT7,
    IOCON_PIN_FUNC_CLKIN,
    IOCON_PIN_FUNC_SCT0_OUT0,
    IOCON_PIN_FUNC_SCT0_OUT1,
    IOCON_PIN_FUNC_SCT0_OUT2,
    IOCON_PIN_FUNC_SCT0_OUT3,
    IOCON_PIN_FUNC_SCT0_OUT4,
    IOCON_PIN_FUNC_SCT0_OUT5,
    IOCON_PIN_FUNC_SCT0_OUT6,
    IOCON_PIN_FUNC_SCT0_OUT7,
    IOCON_PIN_FUNC_SCT0_OUT8,
    IOCON_PIN_FUNC_SCT0_OUT9,
    IOCON_PIN_FUNC_SCT_GPI0,
    IOCON_PIN_FUNC_SCT_GPI1,
    IOCON_PIN_FUNC_SCT_GPI2,
    IOCON_PIN_FUNC_SCT_GPI3,
    IOCON_PIN_FUNC_SCT_GPI4,
    IOCON_PIN_FUNC_SCT_GPI5,
    IOCON_PIN_FUNC_SCT_GPI6,
    IOCON_PIN_FUNC_SCT_GPI7,
    IOCON_PIN_FUNC_SD0_CARD_DET_N,
    IOCON_PIN_FUNC_SD0_CLK,
    IOCON_PIN_FUNC_SD0_CMD,
    IOCON_PIN_FUNC_SD0_D0,
    IOCON_PIN_FUNC_SD0_D1,
    IOCON_PIN_FUNC_SD0_D2,
    IOCON_PIN_FUNC_SD0_D3,
    IOCON_PIN_FUNC_SD0_D4,
    IOCON_PIN_FUNC_SD0_D5,
    IOCON_PIN_FUNC_SD0_D6,
    IOCON_PIN_FUNC_SD0_D7,
    IOCON_PIN_FUNC_SD0_POW_EN,
    IOCON_PIN_FUNC_SD0_WR_PRT,
    IOCON_PIN_FUNC_SD1_BACKEND_PWR,
    IOCON_PIN_FUNC_SD1_CARD_INT_N,
    IOCON_PIN_FUNC_SD1_CLK,
    IOCON_PIN_FUNC_SD1_CMD,
    IOCON_PIN_FUNC_SD1_D0,
    IOCON_PIN_FUNC_SD1_D1,
    IOCON_PIN_FUNC_SD1_D2,
    IOCON_PIN_FUNC_SD1_D3,
    IOCON_PIN_FUNC_SD1_POW_EN,
    IOCON_PIN_FUNC_SWO,
    IOCON_PIN_FUNC_SWCLK,
    IOCON_PIN_FUNC_SWDIO,
    IOCON_PIN_FUNC_USB0_FRAME,
    IOCON_PIN_FUNC_USB0_IDVALUE,
    IOCON_PIN_FUNC_USB0_OVERCURRENTN,
    IOCON_PIN_FUNC_USB0_uart_numPWRN,
    IOCON_PIN_FUNC_USB0_VBUS,
    IOCON_PIN_FUNC_USB1_FRAME,
    IOCON_PIN_FUNC_USB1_LEDN,
    IOCON_PIN_FUNC_USB1_OVERCURRENTN,
    IOCON_PIN_FUNC_USB1_uart_numPWRN,
    IOCON_PIN_FUNC_UTICK_CAP0,
    IOCON_PIN_FUNC_UTICK_CAP1,
    IOCON_PIN_FUNC_UTICK_CAP2,
    IOCON_PIN_FUNC_UTICK_CAP3,
} iocon_pinmux_t;

typedef enum {
    FCX_TXD_SCL_MISO,
    FCX_RXD_SDA_MOSI,
    FCX_CTS_SDAX_SSEL0,
    FCX_RTS_SCLX_SSEL1,
    FCX_SCK,
    FCX_SSEL2,
    FCX_SSEL3,
} flexcomm_pin_t;

static struct hal_uart *
fsl_uart(int uart_num)
{
    if (uart_num >= FSL_FEATURE_SOC_USART_COUNT) {
        return NULL;
    } else {
        return uarts[uart_num];
    }

}

int
hal_uart_init_cbs(int uart_num, hal_uart_tx_char tx_func,
                  hal_uart_tx_done tx_done, hal_uart_rx_char rx_func, void *arg)
{
    struct hal_uart *uart = fsl_uart(uart_num);

    if (uart == NULL) {
        return -1;
    }

    uart->u_rx_func = rx_func;
    uart->u_tx_func = tx_func;
    uart->u_tx_done = tx_done;
    uart->u_func_arg = arg;

    return 0;
}

void
hal_uart_blocking_tx(int uart_num, uint8_t byte)
{
    struct hal_uart *uart = fsl_uart(uart_num);

    if (uart == NULL || !uart->u_configured || !uart->u_open) {
        return;
    }

    USART_WriteBlocking(uart->base, &byte, 1);
}

static int
hal_uart_tx_fill_buf(struct hal_uart *uart)
{
    int data;
    int i;

    for (i = 0; i < sizeof(uart->tx_buffer); i++) {
        data = uart->u_tx_func(uart->u_func_arg);
        if (data < 0) {
            break;
        }
        uart->tx_buffer[i] = data;
    }

    return i;
}

void
hal_uart_start_tx(int uart_num)
{
    struct hal_uart *uart = fsl_uart(uart_num);
    usart_transfer_t transfer;
    int rc;

    if (uart == NULL || !uart->u_configured || !uart->u_open) {
        return;
    }

    /* add data to TX ring buffer */
    if (uart->u_tx_started == 0) {
        rc = hal_uart_tx_fill_buf(uart);
        uart->u_tx_started = rc > 0;
        if (uart->u_tx_started) {
            transfer.txData = uart->tx_buffer;
            transfer.dataSize = rc;
            USART_TransferSendNonBlocking(uart->base, &uart->fsl_handle, &transfer);
        }
    }
}

void
hal_uart_start_rx(int uart_num)
{
    struct hal_uart *uart = fsl_uart(uart_num);
    usart_transfer_t transfer;
    size_t received;
    int sr;
    int rc = 0;


    if (uart == NULL || !uart->u_configured || !uart->u_open) {
        return;
    }

    if (uart->u_rx_stall) {
        __HAL_DISABLE_INTERRUPTS(sr);
        rc = uart->u_rx_func(uart->u_func_arg, uart->u_rx_buf);
        if (rc == 0) {
            uart->u_rx_stall = 0;
            transfer.rxData = &uart->u_rx_buf;
            transfer.dataSize = 1;
            USART_TransferReceiveNonBlocking(uart->base, &uart->fsl_handle, &transfer, &received);
        }
        __HAL_ENABLE_INTERRUPTS(sr);
    }
}

static void
usart_transfer_callback(USART_Type *base, usart_handle_t *handle, status_t status, void *user_data)
{
    struct hal_uart *uart = user_data;
    usart_transfer_t xfer;
    int rc;

    switch (status) {
    case kStatus_USART_TxIdle:
        rc = hal_uart_tx_fill_buf(uart);
        uart->u_tx_started = rc > 0;
        if (uart->u_tx_started) {
            xfer.txData = uart->tx_buffer;
            xfer.dataSize = rc;
            USART_TransferSendNonBlocking(uart->base, &uart->fsl_handle, &xfer);
        } else {
            if (uart->u_tx_done) {
                uart->u_tx_done(uart->u_func_arg);
            }
        }
        break;
    case kStatus_USART_RxIdle:
        rc = uart->u_rx_func(uart->u_func_arg, uart->u_rx_buf);
        if (rc < 0) {
            uart->u_rx_stall = 1;
        } else {
            xfer.rxData = &uart->u_rx_buf;
            xfer.dataSize = 1;
            USART_TransferReceiveNonBlocking(uart->base, &uart->fsl_handle, &xfer, NULL);
        }
        break;
    default:
        break;
    }
}

int
hal_uart_config(int uart_num, int32_t speed, uint8_t databits, uint8_t stopbits,
                enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *uart = fsl_uart(uart_num);
    usart_config_t uconfig;

    if (uart == NULL) {
        return -OS_EINVAL;
    }

    if (!uart->u_configured || uart->u_open) {
        return -OS_EBUSY;
    }

    CLOCK_AttachClk(uart->clk_src);
    /* PIN config (all UARTs use kuart_num_MuxAlt3) */
    CLOCK_EnableClock(uart->p_clock);
    IOCON_PinMuxSet(IOCON, (uart->pin_rx >> 5) & 1, uart->pin_rx & 31, (uart->pin_rx >> 8) | IOCON_DIGITAL_EN);
    IOCON_PinMuxSet(IOCON, (uart->pin_tx >> 5) & 1, uart->pin_tx & 31, (uart->pin_tx >> 8) | IOCON_DIGITAL_EN);

    /* UART CONFIG */
    USART_GetDefaultConfig(&uconfig);
    uconfig.baudRate_Bps = speed;
    uconfig.enableRx = true;
    uconfig.enableTx = true;

    /* TODO: only handles 8 databits currently */

    switch (stopbits) {
    case 1:
        uconfig.stopBitCount = kUSART_OneStopBit;
        break;
    case 2:
        uconfig.stopBitCount = kUSART_TwoStopBit;
        break;
    default:
        return -1;
    }

    switch (parity) {
    case HAL_UART_PARITY_NONE:
        uconfig.parityMode = kUSART_ParityDisabled;
        break;
    case HAL_UART_PARITY_ODD:
        uconfig.parityMode = kUSART_ParityOdd;
        break;
    case HAL_UART_PARITY_EVEN:
        uconfig.parityMode = kUSART_ParityEven;
        break;
    }

    /* TODO: HW flow control not supuart_numed */
    assert(flow_ctl == HAL_UART_FLOW_CTL_NONE);

    uart->u_open = 1;
    uart->u_tx_started = 0;
    uart->u_rx_stall = 1;

    NVIC_SetVector(uart->irqn, (uint32_t)uart->isr);

    /* Initialize UART device */
    USART_Init(uart->base, &uconfig, CLOCK_GetFlexCommClkFreq(uart->clk_src));
    USART_TransferCreateHandle(uart->base, &uart->fsl_handle, usart_transfer_callback, uart);

    hal_uart_start_rx(uart_num);

    return 0;
}

int
hal_uart_close(int uart_num)
{
    struct hal_uart *uart = fsl_uart(uart_num);

    if (uart == NULL || !uart->u_open) {
        return -1;
    }

    uart->u_open = 0;
    USART_DisableInterrupts(uart->base, kUSART_AllInterruptEnables);

    return 0;
}

int
hal_uart_init(int uart_num, void *cfg)
{
    struct hal_uart *uart = fsl_uart(uart_num);

    if (uart == NULL) {
        return -1;
    }
    uart->u_configured = 1;

    return 0;
}
