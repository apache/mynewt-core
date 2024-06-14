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
#include <mcu/stm32_hal.h>
#include <defs/error.h>
#include <mcu/mcu.h>
#include <hal/hal_gpio.h>
#include <bus/bus.h>
#include <bus/bus_debug.h>
#include <bus/bus_driver.h>
#include <bus/drivers/spi_common.h>
#include <bus/drivers/spi_stm32.h>
#include <spidmacfg.h>
#include <stm32_common/stm32_dma.h>

#if MYNEWT_VAL(SPI_0_MASTER) && !defined(SPI1)
#error This MCU does not have SPI1
#endif

#if MYNEWT_VAL(SPI_1_MASTER) && !defined(SPI2)
#error This MCU does not have SPI2
#endif

#if MYNEWT_VAL(SPI_2_MASTER) && !defined(SPI3)
#error This MCU does not have SPI3
#endif

#if MYNEWT_VAL(SPI_3_MASTER) && !defined(SPI4)
#error This MCU does not have SPI4
#endif

#if MYNEWT_VAL(SPI_4_MASTER) && !defined(SPI5)
#error This MCU does not have SPI5
#endif

#if MYNEWT_VAL(SPI_5_MASTER) && !defined(SPI6)
#error This MCU does not have SPI6
#endif

#if MYNEWT_VAL(MCU_STM32U5)
/* DMA peripheral on STM32U5 is called GPDMA and macro to turn it's clock
 * matches this name. For simplicity define old name __HAL_RCC_DMA1_CLK_ENABLE
 * to defintion from U5 hal
 */
#define __HAL_RCC_DMA1_CLK_ENABLE __HAL_RCC_GPDMA1_CLK_ENABLE
#endif

/* Minimum transfer size when DMA should be used, for shorter transfers
 * interrupts are used
 */
#define MIN_DMA_RX_SIZE MYNEWT_VAL(SPI_STM32_MIN_RX_LENGTH_FOR_DMA)
#define MIN_DMA_TX_SIZE MYNEWT_VAL(SPI_STM32_MIN_TX_LENGTH_FOR_DMA)

#if MYNEWT_VAL(SPI_STM32_STAT)
#include "stats/stats.h"
#endif

#if MYNEWT_VAL(SPI_STM32_STAT)
STATS_SECT_START(spi_stm32_stats_section)
    STATS_SECT_ENTRY(read_count)
    STATS_SECT_ENTRY(write_count)
    STATS_SECT_ENTRY(transaction_error_count)
    STATS_SECT_ENTRY(read_bytes)
    STATS_SECT_ENTRY(written_bytes)
    STATS_SECT_ENTRY(dma_transferred_bytes)
STATS_SECT_END

STATS_NAME_START(spi_stm32_stats_section)
    STATS_NAME(spi_stm32_stats_section, read_count)
    STATS_NAME(spi_stm32_stats_section, write_count)
    STATS_NAME(spi_stm32_stats_section, transaction_error_count)
    STATS_NAME(spi_stm32_stats_section, read_bytes)
    STATS_NAME(spi_stm32_stats_section, written_bytes)
    STATS_NAME(spi_stm32_stats_section, dma_transferred_bytes)
STATS_NAME_END(spi_stm32_stats_section)

#define SPI_STATS_INC STATS_INC
#define SPI_STATS_INCN STATS_INCN
#else
#define SPI_STATS_INC(__sectvarname, __var)  do {} while (0)
#define SPI_STATS_INCN(__sectvarname, __var, __n)  do {} while (0)
#endif

/* Driver specific data needed for SPI transfer */
struct spi_stm32_driver_data {
    SPI_HandleTypeDef hspi;
    DMA_HandleTypeDef dmarx;
    DMA_HandleTypeDef dmatx;
    struct bus_spi_dev *dev;
    const struct stm32_spi_hw *hw;
    /* Semaphore used for end of transfer completion notification */
    struct os_sem sem;

#if MYNEWT_VAL(SPI_STM32_STAT)
    STATS_SECT_DECL(spi_stm32_stats_section) stats;
#endif
};

/* Constant data needed for SPI/DMA configuration */
struct stm32_spi_hw {
    uint8_t spi_num;
    IRQn_Type irqn;
    const struct stm32_dma_cfg *dmarx_cfg;
    const struct stm32_dma_cfg *dmatx_cfg;
    void (*irq_handler)(void);
    void (*enable_clock)(bool enable);
    uint32_t (*get_pclk)(void);
};

/*
 * SPI functions can only appear on some pins.  To take burden of specifying pin
 * characteristics of the user, following structures and code allow to
 * use simple pin numbers and SPI function to configure pins in MCU.
 */
enum spi_pin_func {
    SPI_SCK,
    SPI_MOSI,
    SPI_MISO
};

enum spi_alt_num {
    SPI_AF_0,
    SPI_AF_1,
    SPI_AF_2,
    SPI_AF_5 = 5,
    SPI_AF_6,
    SPI_AF_7,
    SPI_AF_INVALID = 0xFF,
};

struct spi_pin_def {
    /** SPI master number (0-5) */
    uint8_t spi_num;
    /** Pin number should be created using MCU_GPIO_PORTx() macro */
    uint8_t pin_num;
    /** Pin function */
    enum spi_pin_func pin_func;
    /** Alternate function number needed for most STM MCUs (except F1) */
    enum spi_alt_num alt_fun;
};

#define SPI_PIN_DEF(_spi_num, _pin, _func, _alt) { _spi_num, _pin, _func, _alt }

/* STMF0 and STML0 have distinct alternate pin functions */
#if MYNEWT_VAL(MCU_STM32L0) || MYNEWT_VAL(MCU_STM32F0)|| MYNEWT_VAL(MCU_STM32G0)
static const struct spi_pin_def spi_pin[] = {
#if MYNEWT_VAL(SPI_0_MASTER)
    SPI_PIN_DEF(0, MCU_GPIO_PORTA(5), SPI_SCK, SPI_AF_0),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(3), SPI_SCK, SPI_AF_0),
    SPI_PIN_DEF(0, MCU_GPIO_PORTE(13), SPI_SCK, SPI_AF_2),

    SPI_PIN_DEF(0, MCU_GPIO_PORTA(7), SPI_MOSI, SPI_AF_0),
    SPI_PIN_DEF(0, MCU_GPIO_PORTA(12), SPI_MOSI, SPI_AF_0),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(5), SPI_MOSI, SPI_AF_0),
    SPI_PIN_DEF(0, MCU_GPIO_PORTE(15), SPI_MOSI, SPI_AF_2),

    SPI_PIN_DEF(0, MCU_GPIO_PORTA(6), SPI_MISO, SPI_AF_0),
    SPI_PIN_DEF(0, MCU_GPIO_PORTA(11), SPI_MISO, SPI_AF_0),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(4), SPI_MISO, SPI_AF_0),
    SPI_PIN_DEF(0, MCU_GPIO_PORTE(14), SPI_MISO, SPI_AF_2),
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    SPI_PIN_DEF(1, MCU_GPIO_PORTB(10), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTB(13), SPI_SCK, SPI_AF_0),
    SPI_PIN_DEF(1, MCU_GPIO_PORTD(1), SPI_SCK, SPI_AF_1),

    SPI_PIN_DEF(1, MCU_GPIO_PORTB(15), SPI_MOSI, SPI_AF_0),
    SPI_PIN_DEF(1, MCU_GPIO_PORTC(3), SPI_MOSI, SPI_AF_2),
    SPI_PIN_DEF(1, MCU_GPIO_PORTD(4), SPI_MOSI, SPI_AF_1),

    SPI_PIN_DEF(1, MCU_GPIO_PORTB(14), SPI_MISO, SPI_AF_0),
    SPI_PIN_DEF(1, MCU_GPIO_PORTC(2), SPI_MISO, SPI_AF_2),
    SPI_PIN_DEF(1, MCU_GPIO_PORTD(3), SPI_MISO, SPI_AF_2),
#endif
};

#else

/*
 * All other MCU seems to have same alternate functions for each type of PIN.
 * For F1 function is specified and later ignored as it is not needed.
 */
static const struct spi_pin_def spi_pin[] = {
#if MYNEWT_VAL(SPI_0_MASTER)
    SPI_PIN_DEF(0, MCU_GPIO_PORTA(5), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(3), SPI_SCK, SPI_AF_5),

    SPI_PIN_DEF(0, MCU_GPIO_PORTA(7), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(5), SPI_MOSI, SPI_AF_5),

    SPI_PIN_DEF(0, MCU_GPIO_PORTA(6), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(4), SPI_MISO, SPI_AF_5),
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    SPI_PIN_DEF(1, MCU_GPIO_PORTB(10), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTB(13), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTC(7), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTD(3), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTI(1), SPI_SCK, SPI_AF_5),

    SPI_PIN_DEF(1, MCU_GPIO_PORTB(15), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTC(3), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTI(3), SPI_MOSI, SPI_AF_5),

    SPI_PIN_DEF(1, MCU_GPIO_PORTB(14), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTC(2), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTI(2), SPI_MISO, SPI_AF_5),
#endif

#if MYNEWT_VAL(SPI_2_MASTER)
    SPI_PIN_DEF(2, MCU_GPIO_PORTB(3), SPI_SCK, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTC(10), SPI_SCK, SPI_AF_6),

    SPI_PIN_DEF(2, MCU_GPIO_PORTB(5), SPI_MOSI, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTC(12), SPI_MOSI, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTD(6), SPI_MOSI, SPI_AF_5),

    SPI_PIN_DEF(2, MCU_GPIO_PORTB(4), SPI_MISO, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTC(11), SPI_MISO, SPI_AF_6),
#endif

#if MYNEWT_VAL(SPI_3_MASTER)
    SPI_PIN_DEF(3, MCU_GPIO_PORTE(2), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(3, MCU_GPIO_PORTE(12), SPI_SCK, SPI_AF_5),

    SPI_PIN_DEF(3, MCU_GPIO_PORTE(6), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(3, MCU_GPIO_PORTE(14), SPI_MOSI, SPI_AF_5),

    SPI_PIN_DEF(3, MCU_GPIO_PORTE(5), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(3, MCU_GPIO_PORTE(13), SPI_MISO, SPI_AF_5),
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
    SPI_PIN_DEF(4, MCU_GPIO_PORTF(7), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(4, MCU_GPIO_PORTH(6), SPI_SCK, SPI_AF_5),

    SPI_PIN_DEF(4, MCU_GPIO_PORTF(9), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(4, MCU_GPIO_PORTF(11), SPI_MOSI, SPI_AF_5),

    SPI_PIN_DEF(4, MCU_GPIO_PORTF(8), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(4, MCU_GPIO_PORTH(7), SPI_MISO, SPI_AF_5),
#endif

#if MYNEWT_VAL(SPI_5_MASTER)
    SPI_PIN_DEF(5, MCU_GPIO_PORTA(5), SPI_SCK, SPI_AF_7),
    SPI_PIN_DEF(5, MCU_GPIO_PORTB(3), SPI_SCK, SPI_AF_7),
    SPI_PIN_DEF(5, MCU_GPIO_PORTG(13), SPI_SCK, SPI_AF_5),

    SPI_PIN_DEF(5, MCU_GPIO_PORTA(7), SPI_MOSI, SPI_AF_7),
    SPI_PIN_DEF(5, MCU_GPIO_PORTB(5), SPI_MOSI, SPI_AF_7),
    SPI_PIN_DEF(5, MCU_GPIO_PORTG(14), SPI_MOSI, SPI_AF_5),

    SPI_PIN_DEF(5, MCU_GPIO_PORTA(6), SPI_MISO, SPI_AF_7),
    SPI_PIN_DEF(5, MCU_GPIO_PORTB(4), SPI_MISO, SPI_AF_7),
    SPI_PIN_DEF(5, MCU_GPIO_PORTG(12), SPI_MISO, SPI_AF_5),
#endif
};
#endif

/**
 * Function returns alternate function number.
 * @param spi_num   SPI_x_MASTER number
 * @param pin       pin number
 * @param func      required pin function
 * @return          alternate function number of SPI_AF_INVALID if pin
 *                  can't be setup for this function.
 */
enum spi_alt_num
spi_stm32_pin_af(int spi_num, int pin, enum spi_pin_func func)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(spi_pin); ++i) {
        if (spi_pin[i].spi_num == spi_num &&
            spi_pin[i].pin_num == pin &&
            spi_pin[i].pin_func == func) {
            return spi_pin[i].alt_fun;
        }
    }
    return SPI_AF_INVALID;
}

/* SPI1 specific section */
#if MYNEWT_VAL(SPI_0_MASTER)
static void spi1_irq_handler(void);
static void spi1_clock_enable(bool enable);

static const struct stm32_spi_hw stm32_spi1_hw = {
    .spi_num = 0,
    .irqn = SPI1_IRQn,
    .irq_handler = spi1_irq_handler,
    .enable_clock = spi1_clock_enable,
#if MYNEWT_VAL(MCU_STM32F0) || MYNEWT_VAL(MCU_STM32G0)
    .get_pclk = HAL_RCC_GetPCLK1Freq,
#else
    .get_pclk = HAL_RCC_GetPCLK2Freq,
#endif
    .dmarx_cfg = &MYNEWT_VAL(SPI1_RX_DMA),
    .dmatx_cfg = &MYNEWT_VAL(SPI1_TX_DMA),
};

static struct spi_stm32_driver_data spi1_dev_data = {
    .hw = &stm32_spi1_hw,
    .hspi.Instance = SPI1,
};

static void
spi1_clock_enable(bool enable)
{
    if (enable) {
        __HAL_RCC_SPI1_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI1_CLK_DISABLE();
    }
}

static void
spi1_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_SPI_IRQHandler(&spi1_dev_data.hspi);

    os_trace_isr_exit();
}
#endif

/* SPI2 specific section */
#if MYNEWT_VAL(SPI_1_MASTER)
static void spi2_irq_handler(void);
static void spi2_clock_enable(bool enable);

static const struct stm32_spi_hw stm32_spi2_hw = {
    .spi_num = 1,
    .irqn = SPI2_IRQn,
    .irq_handler = spi2_irq_handler,
    .enable_clock = spi2_clock_enable,
    .get_pclk = HAL_RCC_GetPCLK1Freq,
    .dmarx_cfg = &MYNEWT_VAL(SPI2_RX_DMA),
    .dmatx_cfg = &MYNEWT_VAL(SPI2_TX_DMA),
};

static struct spi_stm32_driver_data spi2_dev_data = {
    .hw = &stm32_spi2_hw,
    .hspi.Instance = SPI2,
};

static void
spi2_clock_enable(bool enable)
{
    if (enable) {
        __HAL_RCC_SPI2_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI2_CLK_DISABLE();
    }
}

static void
spi2_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_SPI_IRQHandler(&spi2_dev_data.hspi);

    os_trace_isr_exit();
}
#endif

/* SPI3 specific section */
#if MYNEWT_VAL(SPI_2_MASTER)
static void spi3_irq_handler(void);
static void spi3_clock_enable(bool enable);

static const struct stm32_spi_hw stm32_spi3_hw = {
    .spi_num = 2,
    .irqn = SPI3_IRQn,
    .irq_handler = spi3_irq_handler,
    .enable_clock = spi3_clock_enable,
    .get_pclk = HAL_RCC_GetPCLK1Freq,
    .dmarx_cfg = &MYNEWT_VAL(SPI3_RX_DMA),
    .dmatx_cfg = &MYNEWT_VAL(SPI3_TX_DMA),
};

struct spi_stm32_driver_data spi3_dev_data = {
    .hw = &stm32_spi3_hw,
    .hspi.Instance = SPI3,
};

static void
spi3_clock_enable(bool enable)
{
    if (enable) {
        __HAL_RCC_SPI3_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI3_CLK_DISABLE();
    }
}

static void
spi3_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_SPI_IRQHandler(&spi3_dev_data.hspi);

    os_trace_isr_exit();
}
#endif

/* SPI4 specific section */
#if MYNEWT_VAL(SPI_3_MASTER)
static void spi4_irq_handler(void);
static void spi4_clock_enable(bool enable);

static const struct stm32_spi_hw stm32_spi4_hw = {
    .spi_num = 3,
    .irqn = SPI4_IRQn,
    .irq_handler = spi4_irq_handler,
    .enable_clock = spi4_clock_enable,
    .get_pclk = HAL_RCC_GetPCLK2Freq,
    .dmarx_cfg = &MYNEWT_VAL(SPI4_RX_DMA),
    .dmatx_cfg = &MYNEWT_VAL(SPI4_TX_DMA),
};

struct spi_stm32_driver_data spi4_dev_data = {
    .hw = &stm32_spi4_hw,
    .hspi.Instance = SPI4,
};

static void
spi4_clock_enable(bool enable)
{
    if (enable) {
        __HAL_RCC_SPI4_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI4_CLK_DISABLE();
    }
}

static void
spi4_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_SPI_IRQHandler(&spi4_dev_data.hspi);

    os_trace_isr_exit();
}

#endif

/* SPI5 specific section */
#if MYNEWT_VAL(SPI_4_MASTER)
static void spi5_irq_handler(void);
static void spi5_clock_enable(bool enable);

const struct stm32_spi_hw stm32_spi5_hw = {
    .spi_num = 4,
    .irqn = SPI5_IRQn,
    .irq_handler = spi5_irq_handler,
    .enable_clock = spi5_clock_enable,
    .get_pclk = HAL_RCC_GetPCLK2Freq,
    .dmarx_cfg = &MYNEWT_VAL(SPI5_RX_DMA),
    .dmatx_cfg = &MYNEWT_VAL(SPI5_TX_DMA),
};

struct spi_stm32_driver_data spi5_dev_data = {
    .hw = &stm32_spi5_hw,
    .hspi.Instance = SPI5,
};

static void
spi5_clock_enable(bool enable)
{
    if (enable) {
        __HAL_RCC_SPI5_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI5_CLK_DISABLE();
    }
}

static void
spi5_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_SPI_IRQHandler(&spi5_dev_data.hspi);

    os_trace_isr_exit();
}
#endif

/* SPI6 specific section */
#if MYNEWT_VAL(SPI_5_MASTER)
static void spi6_irq_handler(void);
static void spi6_clock_enable(bool enable);

static const struct stm32_spi_hw stm32_spi6_hw = {
    .spi_num = 5,
    .irqn = SPI6_IRQn,
    .irq_handler = spi6_irq_handler,
    .enable_clock = spi6_clock_enable,
    .get_pclk = HAL_RCC_GetPCLK2Freq,
    .dmarx_cfg = &MYNEWT_VAL(SPI6_RX_DMA),
    .dmatx_cfg = &MYNEWT_VAL(SPI6_TX_DMA),
};

static struct spi_stm32_driver_data spi6_dev_data = {
    .hw = &stm32_spi6_hw,
    .hspi.Instance = SPI6,
};

static void
spi6_clock_enable(bool enable)
{
    if (enable) {
        __HAL_RCC_SPI6_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI6_CLK_DISABLE();
    }
}

static void
spi6_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_SPI_IRQHandler(&spi6_dev_data.hspi);

    os_trace_isr_exit();
}
#endif

static inline struct spi_stm32_driver_data *
driver_data(struct bus_spi_dev *dev)
{
    switch (dev->cfg.spi_num) {
#if MYNEWT_VAL(SPI_0_MASTER)
    case 0:
        return &spi1_dev_data;
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    case 1:
        return &spi2_dev_data;
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    case 2:
        return &spi3_dev_data;
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
    case 3:
        return &spi4_dev_data;
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
    case 4:
        return &spi5_dev_data;
#endif
#if MYNEWT_VAL(SPI_5_MASTER)
    case 5:
        return &spi6_dev_data;
#endif
    default:
        assert(0);
        return NULL;
    }
}

static int
spi_stm32_init_node(struct bus_dev *bdev, struct bus_node *bnode, void *arg)
{
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct bus_spi_node_cfg *cfg = arg;
    (void)bdev;

    BUS_DEBUG_POISON_NODE(node);

    node->pin_cs = cfg->pin_cs;
    node->freq = cfg->freq;
    node->quirks = cfg->quirks;
    node->data_order = cfg->data_order;
    node->mode = cfg->mode;

    if (node->pin_cs >= 0) {
        hal_gpio_init_out(node->pin_cs, 1);
    }

    return 0;
}

static int
spi_stm32_configure(struct bus_dev *bdev, struct bus_node *bnode)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct bus_spi_node *current_node = (struct bus_spi_node *)bdev->configured_for;
    struct spi_stm32_driver_data *dd;
    const struct stm32_spi_hw *hw;
    uint32_t pclk;
    uint32_t freq;
    int prescaler;
    int rc = 0;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    dd = driver_data(dev);

    if (current_node &&
        current_node->freq == node->freq &&
        current_node->data_order == node->data_order &&
        current_node->mode == node->mode) {
        goto end;
    }

    /* Change from kHz to Hz */
    freq = node->freq * 1000;
    hw = dd->hw;
    pclk = hw->get_pclk() / 2;
    /* Find prescaler so frequency does not exceed requested one. */
    for (prescaler = 0; freq < pclk; ++prescaler) {
        pclk >>= 1;
    }

    if (prescaler > 7) {
        rc = SYS_EINVAL;
    } else {
#if MYNEWT_VAL(MCU_STM32H7) || MYNEWT_VAL(MCU_STM32U5)
        dd->hspi.Init.BaudRatePrescaler = prescaler << SPI_CFG1_MBR_Pos;
#else
        dd->hspi.Init.BaudRatePrescaler = prescaler << SPI_CR1_BR_Pos;
#endif
        dd->hspi.Init.CLKPolarity = (node->mode == BUS_SPI_MODE_0 || node->mode == BUS_SPI_MODE_1) ?
                                    SPI_POLARITY_LOW : SPI_POLARITY_HIGH;
        dd->hspi.Init.CLKPhase = (node->mode == BUS_SPI_MODE_0 || node->mode == BUS_SPI_MODE_2) ?
                                 SPI_PHASE_1EDGE : SPI_PHASE_2EDGE;
        dd->hspi.Init.FirstBit = node->data_order == BUS_SPI_DATA_ORDER_MSB ?
                                 SPI_FIRSTBIT_MSB : SPI_FIRSTBIT_LSB;
#if defined(SPI_MASTER_KEEP_IO_STATE_ENABLE)
        dd->hspi.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
#endif
    }
    if (HAL_OK != HAL_SPI_Init(&dd->hspi)) {
        rc = SYS_EINVAL;
    }

    __HAL_SPI_ENABLE(&dd->hspi);

end:
    return rc;
}

void
HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    struct spi_stm32_driver_data *dd = (struct spi_stm32_driver_data *)hspi;

    os_sem_release(&dd->sem);
}

void
HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    struct spi_stm32_driver_data *dd = (struct spi_stm32_driver_data *)hspi;

    os_sem_release(&dd->sem);
}

void
HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    struct spi_stm32_driver_data *dd = (struct spi_stm32_driver_data *)hspi;

    os_sem_release(&dd->sem);
}

static int
spi_stm32_read(struct bus_dev *bdev, struct bus_node *bnode,
               uint8_t *buf, uint16_t length, os_time_t timeout,
               uint16_t flags)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct spi_stm32_driver_data *dd;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    dd = driver_data(dev);

    if (node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 0);
    }

    SPI_STATS_INC(dd->stats, read_count);

    if (MYNEWT_VAL(OS_SCHEDULING)) {
        assert(os_sem_get_count(&dd->sem) == 0);

        if (MIN_DMA_RX_SIZE >= 0 && length >= MIN_DMA_RX_SIZE) {
            HAL_SPI_Receive_DMA(&dd->hspi, (uint8_t *)buf, length);
        } else {
            HAL_SPI_Receive_IT(&dd->hspi, (uint8_t *)buf, length);
        }

        rc = os_sem_pend(&dd->sem, timeout);

        if (rc) {
            HAL_SPI_Abort(&dd->hspi);
            SPI_STATS_INC(dd->stats, transaction_error_count);
        } else {
            SPI_STATS_INCN(dd->stats, read_bytes, length);
        }

        rc = os_error_to_sys(rc);
    } else {
        rc = HAL_SPI_Receive(&dd->hspi, (uint8_t *)buf, length, timeout);
        if (rc != HAL_OK) {
            rc = SYS_EIO;
        }
    }

    if ((rc != 0 || !(flags & BUS_F_NOSTOP)) && node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 1);
    }

    return rc;
}

static int
spi_stm32_write(struct bus_dev *bdev, struct bus_node *bnode,
                const uint8_t *buf, uint16_t length, os_time_t timeout,
                uint16_t flags)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct spi_stm32_driver_data *dd;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    dd = driver_data(dev);

    SPI_STATS_INC(dd->stats, write_count);

    /* Activate CS */
    if (node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 0);
    }

    if (MYNEWT_VAL(OS_SCHEDULING)) {
        assert(os_sem_get_count(&dd->sem) == 0);

        if (MIN_DMA_TX_SIZE >= 0 && length >= MIN_DMA_TX_SIZE) {
            HAL_SPI_Transmit_DMA(&dd->hspi, (uint8_t *)buf, length);
        } else {
            HAL_SPI_Transmit_IT(&dd->hspi, (uint8_t *)buf, length);
        }

        rc = os_sem_pend(&dd->sem, timeout);

        if (rc) {
            HAL_SPI_Abort(&dd->hspi);
            SPI_STATS_INC(dd->stats, transaction_error_count);
        } else {
            SPI_STATS_INCN(dd->stats, written_bytes, length);
        }

        rc = os_error_to_sys(rc);
    } else {
        rc = HAL_SPI_Transmit(&dd->hspi, (uint8_t *)buf, length, timeout);
        if (rc != HAL_OK) {
            rc = SYS_EIO;
        }
    }
    /* Deactivate CS if needed */
    if ((rc != 0 || !(flags & BUS_F_NOSTOP)) && node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 1);
    }

    return rc;
}

static int
spi_stm32_duplex_write_read(struct bus_dev *bdev, struct bus_node *bnode,
                            const uint8_t *wbuf, uint8_t *rbuf, uint16_t length,
                            os_time_t timeout, uint16_t flags)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct spi_stm32_driver_data *dd;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    dd = driver_data(dev);

    /* Activate CS */
    if (node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 0);
    }

    SPI_STATS_INC(dd->stats, write_count);

    if (MYNEWT_VAL(OS_SCHEDULING)) {
        assert(os_sem_get_count(&dd->sem) == 0);

        if (MIN_DMA_TX_SIZE >= 0 && length >= MIN_DMA_TX_SIZE) {
            HAL_SPI_TransmitReceive_DMA(&dd->hspi, (uint8_t *)wbuf, rbuf, length);
        } else {
            HAL_SPI_TransmitReceive_IT(&dd->hspi, (uint8_t *)wbuf, rbuf, length);
        }

        rc = os_sem_pend(&dd->sem, timeout);

        if (rc) {
            HAL_SPI_Abort(&dd->hspi);
            SPI_STATS_INC(dd->stats, transaction_error_count);
        } else {
            if (MIN_DMA_TX_SIZE >= 0 && length >= MIN_DMA_TX_SIZE) {
                SPI_STATS_INCN(dd->stats, dma_transferred_bytes, length);
            }
            SPI_STATS_INCN(dd->stats, read_bytes, length);
            SPI_STATS_INCN(dd->stats, written_bytes, length);
        }

        rc = os_error_to_sys(rc);
    } else {
        rc = HAL_SPI_TransmitReceive(&dd->hspi, (uint8_t *)wbuf, rbuf, length, timeout);
        if (rc != HAL_OK) {
            rc = SYS_EIO;
        }
    }

    /* Deactivate CS if needed */
    if ((rc != 0 || !(flags & BUS_F_NOSTOP)) && node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 1);
    }

    return rc;
}

static int
spi_stm32_enable(struct bus_dev *bdev)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct spi_stm32_driver_data *dd;

    BUS_DEBUG_VERIFY_DEV(dev);

    dd = driver_data(dev);
    dd->hw->enable_clock(true);

    return 0;
}

static int
spi_stm32_disable(struct bus_dev *bdev)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct spi_stm32_driver_data *dd;

    BUS_DEBUG_VERIFY_DEV(dev);

    dd = driver_data(dev);
    HAL_SPI_DeInit(&dd->hspi);
    dd->hw->enable_clock(false);

    return 0;
}

static const struct bus_dev_ops bus_spi_stm32_ops = {
    .init_node = spi_stm32_init_node,
    .enable = spi_stm32_enable,
    .configure = spi_stm32_configure,
    .read = spi_stm32_read,
    .write = spi_stm32_write,
    .disable = spi_stm32_disable,
    .duplex_write_read = spi_stm32_duplex_write_read,
};

/* Helper function to setup interrupt handler for SPI and DMA */
static void
stm32_init_interrupt(uint8_t irqn, uint8_t pri, void (*handler)(void))
{
    NVIC_DisableIRQ(irqn);

    NVIC_SetVector(irqn, (uint32_t)handler);
    NVIC_SetPriority(irqn, pri);
    NVIC_ClearPendingIRQ(irqn);

    NVIC_EnableIRQ(irqn);
}

int
bus_spi_stm32_dev_init_func(struct os_dev *odev, void *arg)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)odev;
    struct bus_spi_dev_cfg *cfg = arg;
    struct spi_stm32_driver_data *dd;
#if MYNEWT_VAL(SPI_STM32_STAT)
    char *stats_name;
#endif
    int rc;
    enum spi_alt_num af;
    const struct stm32_spi_hw *spi_hw;

    BUS_DEBUG_POISON_DEV(dev);

    dev->cfg = *cfg;
    dd = driver_data(dev);
    if (dd == NULL) {
        return SYS_EINVAL;
    }
    assert(dd->dev == NULL);
    if (dd->dev) {
        return SYS_EALREADY;
    }

    spi_hw = dd->hw;
    dd->dev = dev;

    af = spi_stm32_pin_af(cfg->spi_num, cfg->pin_sck, SPI_SCK);
    assert(SPI_AF_INVALID != af);
    hal_gpio_init_af(cfg->pin_sck, af, HAL_GPIO_PULL_NONE, 0);

    af = spi_stm32_pin_af(cfg->spi_num, cfg->pin_mosi, SPI_MOSI);
    assert(SPI_AF_INVALID != af);
    hal_gpio_init_af(cfg->pin_mosi, af, HAL_GPIO_PULL_NONE, 0);

    af = spi_stm32_pin_af(cfg->spi_num, cfg->pin_miso, SPI_MISO);
    assert(SPI_AF_INVALID != af);
    hal_gpio_init_af(cfg->pin_miso, af, HAL_GPIO_PULL_NONE, 0);

    dd->hspi.Init.Mode = SPI_MODE_MASTER;
    dd->hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    dd->hspi.Init.Direction = SPI_DIRECTION_2LINES;
    dd->hspi.Init.NSS = SPI_NSS_SOFT;
    dd->hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    dd->hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
#ifdef SPI_MASTER_KEEP_IO_STATE_ENABLE
    dd->hspi.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
#endif

    if (MIN_DMA_RX_SIZE >= 0 || MIN_DMA_TX_SIZE >= 0) {
        dd->dmarx.Instance = spi_hw->dmarx_cfg->regs;
        dd->dmatx.Instance = spi_hw->dmatx_cfg->regs;
        dd->dmarx.Init = spi_hw->dmarx_cfg->init;
        dd->dmatx.Init = spi_hw->dmatx_cfg->init;

        __HAL_LINKDMA(&dd->hspi, hdmarx, dd->dmarx);
        __HAL_LINKDMA(&dd->hspi, hdmatx, dd->dmatx);

        if (spi_hw->dmarx_cfg->dma_ch <= DMA1_CH7) {
            __HAL_RCC_DMA1_CLK_ENABLE();
        } else {
#ifdef __HAL_RCC_DMA2_CLK_ENABLE
            __HAL_RCC_DMA2_CLK_ENABLE();
#endif
        }
#ifdef __HAL_RCC_DMAMUX1_CLK_ENABLE
        __HAL_RCC_DMAMUX1_CLK_ENABLE();
#endif

        if (stm32_dma_acquire_channel(spi_hw->dmarx_cfg->dma_ch, &dd->dmarx) == SYS_EOK) {
            HAL_DMA_Init(&dd->dmarx);
            stm32_init_interrupt(spi_hw->dmarx_cfg->irqn, 0, spi_hw->dmarx_cfg->irq_handler);
        }

        if (stm32_dma_acquire_channel(spi_hw->dmatx_cfg->dma_ch, &dd->dmatx) == SYS_EOK) {
            HAL_DMA_Init(&dd->dmatx);
            stm32_init_interrupt(spi_hw->dmatx_cfg->irqn, 0, spi_hw->dmatx_cfg->irq_handler);
        }
    }

    stm32_init_interrupt(spi_hw->irqn, 0, spi_hw->irq_handler);

    if (MYNEWT_VAL(OS_SCHEDULING)) {
        os_sem_init(&dd->sem, 0);
    }

#if MYNEWT_VAL(SPI_STM32_STAT)
    asprintf(&stats_name, "spi_stm32_%d", cfg->spi_num);
    rc = stats_init_and_reg(STATS_HDR(dd->stats),
                            STATS_SIZE_INIT_PARMS(dd->stats, STATS_SIZE_32),
                            STATS_NAME_INIT_PARMS(spi_stm32_stats_section),
                            stats_name);
    assert(rc == 0);
#endif

    rc = bus_dev_init_func(odev, (void *)&bus_spi_stm32_ops);
    assert(rc == 0);

    return 0;
}

