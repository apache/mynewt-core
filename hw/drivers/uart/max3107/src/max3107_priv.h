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

#ifndef __DRIVERS_MAX3107_PRIV_H_
#define __DRIVERS_MAX3107_PRIV_H_

#include <stdint.h>
#include <uart/uart.h>
#include <hal/hal_spi.h>
#if MYNEWT_VAL(MAX3107_STATS)
#include "stats/stats.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX3107_REG_RHR             0x00
#define MAX3107_REG_THR             0x00

#define MAX3107_REG_IRQEN           0x01
#define MAX3107_REG_ISR             0x02
#define MAX3107_REG_LSRINTEN        0x03
#define MAX3107_REG_LSR             0x04
#define MAX3107_REG_SPCLCHRINTEN    0x05
#define MAX3107_REG_SPCLCHARINT     0x06
#define MAX3107_REG_STSINTEN        0x07
#define MAX3107_REG_STSINT          0x08

#define MAX3107_REG_MODE1           0x09
#define MAX3107_REG_MODE2           0x0A
#define MAX3107_REG_LCR             0x0B
#define MAX3107_REG_RXTIMEOUT       0x0C
#define MAX3107_REG_HDPLXDELAY      0x0D
#define MAX3107_REG_IRDA            0x0E

#define MAX3107_REG_FLOWLVL         0x0F
#define MAX3107_REG_FIFOTRGLVL      0x10
#define MAX3107_REG_TXFIFOLVL       0x11
#define MAX3107_REG_RXFIFOLVL       0x12

#define MAX3107_REG_FLOWCTRL        0x13
#define MAX3107_REG_XON1            0x14
#define MAX3107_REG_XON2            0x15
#define MAX3107_REG_XOFF1           0x16
#define MAX3107_REG_XOFF2           0x17

#define MAX3107_REG_GPIOCONFG       0x18
#define MAX3107_REG_GPIODATA        0x19

#define MAX3107_REG_PLLCONFIG       0x1A
#define MAX3107_REG_BRGCONFIG       0x1B
#define MAX3107_REG_DIVLSB          0x1C
#define MAX3107_REG_DIVMSB          0x1D
#define MAX3107_REG_CLKSOURCE       0x1E

#define MAX3107_REG_REVID           0x1F

#define IRQEN_CTSIEN                0x80
#define IRQEN_RXEMTYIEN             0x40
#define IRQEN_TXEMTYIEN             0x20
#define IRQEN_TXTRGIEN              0x10
#define IRQEN_RXTRGIEN              0x08
#define IRQEN_STSIEN                0x04
#define IRQEN_SPCLCHRLEN            0x02
#define IRQEN_LSRERRIEN             0x01

#define ISR_CTSINT                  0x80
#define ISR_RXEMPTYINT              0x40
#define ISR_TXEMPTYINT              0x20
#define ISR_TFIFOTRIGINT            0x10
#define ISR_RFIFOTRIGINT            0x08
#define ISR_STSINT                  0x04
#define ISR_SPCHARINT               0x02
#define ISR_LSRERRINT               0x01

#define LSRINTEN_NOISEINTEN         0x20
#define LSRINTEN_RBREAKIEN          0x10
#define LSRINTEN_FRAMEERRIEN        0x08
#define LSRINTEN_PARITYIEN          0x04
#define LSRINTEN_ROVERRIEN          0x02
#define LSRINTEN_RTIMEOUTIEN        0x01

#define LSR_CTSBIT                  0x80
#define LSR_RXNOISE                 0x20
#define LSR_RXBREAK                 0x10
#define LSR_FRAMEERR                0x08
#define LSR_RXPARITYERR             0x04
#define LSR_RXOVERRUN               0x02
#define LSR_RTIMEOUT                0x01
#define LSR_RXERROOR                (LSR_FRAMEERR | LSR_RXPARITYERR | LSR_RXOVERRUN)

#define SPCLCHRINTEN_MLTDRPINTEN    0x20
#define SPCLCHRINTEN_RBREAKINTEN    0x10
#define SPCLCHRINTEN_XOFF2INTEN     0x08
#define SPCLCHRINTEN_XOFF1INTEN     0x04
#define SPCLCHRINTEN_XON2INTEN      0x02
#define SPCLCHRINTEN_XON1INTEN      0x01

#define SPCLCHRINT_MLTDRPINT        0x20
#define SPCLCHRINT_RBREAKINT        0x10
#define SPCLCHRINT_XOFF2INT         0x08
#define SPCLCHRINT_XOFF1INT         0x04
#define SPCLCHRINT_XON2INT          0x02
#define SPCLCHRINT_XON1INT          0x01

#define MODE1_IRQSEL                0x80
#define MODE1_AUTOSLEEP             0x40
#define MODE1_FORCEDSLEEP           0x20
#define MODE1_TRNSCVCTRL            0x10
#define MODE1_RTSHIZ                0x08
#define MODE1_TXHIZ                 0x04
#define MODE1_TXDISABL              0x02
#define MODE1_RXDISABL              0x01

#define MODE2_ECHOSUPRS             0x80
#define MODE2_MULTIDROP             0x40
#define MODE2_LOOPBACK              0x20
#define MODE2_SPECIALCHR            0x10
#define MODE2_RXEMTYINV             0x08
#define MODE2_RXTRIGINV             0x04
#define MODE2_FIFORST               0x02
#define MODE2_RST                   0x01

#define LCR_RTS                     0x80
#define LCR_TXBREAK                 0x40
#define LCR_FORCEPARITY             0x20
#define LCR_EVENPARITY              0x10
#define LCR_PARITYEN                0x08
#define LCR_STOPBITS                0x04
#define LCR_LENGTH                  0x03

#define FLOWLVL_RESUME              0xF0
#define FLOWLVL_HALT                0x0F

#define FIFOTRGLVL_RXTRIG           0xF0
#define FIFOTRGLVL_TXTRIG           0x0F

#define FLOWCTRL_SWFLOW             0xF0
#define FLOWCTRL_SWFLOWEN           0x08
#define FLOWCTRL_GPIADDR            0x04
#define FLOWCTRL_AUTOCTS            0x02
#define FLOWCTRL_AUTORTS            0x01

#define BRGCONFIG_4XMODE            0x20
#define BRGCONFIG_2XMODE            0x10
#define BRGCONFIG_FRACT             0x0F

#define CLCSOURCE_CLKTORTS          0x80
#define CLCSOURCE_CLOCKEN           0x10
#define CLCSOURCE_PLLBYPASS         0x08
#define CLCSOURCE_PLLEN             0x04
#define CLCSOURCE_CRYSTALEN         0x02

struct max3107_interrupts {
    uint8_t irq_en;
    uint8_t isr;
    uint8_t lsr_int_en;
    uint8_t lsr;
    uint8_t spcl_chr_int_en;
    uint8_t spcl_char_int;
    uint8_t sts_int_en;
    uint8_t sts_int;
} __attribute__((packed));

struct max3107_uart_modes {
    uint8_t mode1;
    uint8_t mode2;
    uint8_t lcr;
    uint8_t rxtimeout;
    uint8_t hdplxdelay;
    uint8_t irda;
} __attribute__((packed));

struct max3107_fifo_control {
    uint8_t flow_lvl;
    uint8_t fifo_trg_lvl;
    uint8_t tx_fifo_lvl;
    uint8_t rx_fifo_lvl;
} __attribute__((packed));

struct max3107_flow_control {
    uint8_t flow_ctrl;
    uint8_t xon1;
    uint8_t xon2;
    uint8_t xoff1;
    uint8_t xoff2;
} __attribute__((packed));

struct max3107_gpios {
    uint8_t gpio_confg;
    uint8_t gpio_data;
} __attribute__((packed));

struct max3107_clock_cfg {
    uint8_t pll_config;
    uint8_t brg_config;
    uint8_t div_lsb;
    uint8_t div_msb;
    uint8_t clk_source;
} __attribute__((packed));

struct max3107_regs {
    struct max3107_interrupts ints;
    struct max3107_uart_modes modes;
    struct max3107_fifo_control fifo;
    struct max3107_flow_control flow;
    struct max3107_gpios gpio;
    struct max3107_clock_cfg clock;
} __attribute__((packed));

#if MYNEWT_VAL(MAX3107_STATS)
STATS_SECT_START(max3107_stats_section)
    STATS_SECT_ENTRY(lock_timeouts)
    STATS_SECT_ENTRY(uart_read_ops)
    STATS_SECT_ENTRY(uart_read_errors)
    STATS_SECT_ENTRY(uart_breaks)
    STATS_SECT_ENTRY(uart_read_bytes)
    STATS_SECT_ENTRY(uart_write_ops)
    STATS_SECT_ENTRY(uart_write_errors)
    STATS_SECT_ENTRY(uart_write_bytes)
STATS_SECT_END

#define MAX3107_STATS_INC STATS_INC
#define MAX3107_STATS_INCN STATS_INCN

#else

#define MAX3107_STATS_INC(__sectvarname, __var)  do {} while (0)
#define MAX3107_STATS_INCN(__sectvarname, __var, __n)  do {} while (0)

#endif

struct max3107_dev {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node dev;
#else
    struct os_dev dev;
#endif
    struct os_mutex lock;

    struct max3107_cfg cfg;

    uint32_t real_baudrate;

    const struct max3107_client *client;
    bool writable_notified;
    bool readable_notified;

    bool irq_pending;
    bool recheck_rx_fifo_level;
    /* Shadow copy of device registers. */
    struct max3107_regs regs;
    struct os_event irq_event;
    struct os_eventq *event_queue;

    struct uart_dev uart;
    uart_tx_char uc_tx_char;
    uart_rx_char uc_rx_char;
    uart_tx_done uc_tx_done;
    void *uc_cb_arg;
    uint8_t rx_buf[MYNEWT_VAL(MAX3107_UART_RX_BUFFER_SIZE)];
    uint8_t tx_buf[MYNEWT_VAL(MAX3107_UART_TX_BUFFER_SIZE)];
    uint8_t rx_buf_count;
    uint8_t tx_buf_count;
#if MYNEWT_VAL(MAX3107_STATS)
    STATS_SECT_DECL(max3107_stats_section) stats;
#endif
};

int max3107_write_regs(struct max3107_dev *dev, uint8_t addr, const uint8_t *buf, uint32_t size);
int max3107_read_regs(struct max3107_dev *dev, uint8_t addr, void *buf, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __DRIVERS_MAX3107_PRIV_H_ */
