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
#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "stm32f407xx.h"
#include "stm32f4xx_hal_gpio_ex.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_adc.h"
#include "flash_map/flash_map.h"
#if MYNEWT_VAL(TRNG)
#include "trng/trng.h"
#include "trng_stm32/trng_stm32.h"
#endif
#if MYNEWT_VAL(UART_0)
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#endif
#if MYNEWT_VAL(ADC_1) || MYNEWT_VAL(ADC_2) || MYNEWT_VAL(ADC_3)
#include "adc_stm32f4/adc_stm32f4.h"
#endif
#include "hal/hal_i2c.h"
#include "hal/hal_timer.h"
#include "hal/hal_bsp.h"
#include "hal/hal_gpio.h"
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
#include "hal/hal_spi.h"
#endif
#if MYNEWT_VAL(ETH_0)
#include "stm32_eth/stm32_eth.h"
#include "stm32_eth/stm32_eth_cfg.h"
#endif
#include "mcu/stm32_hal.h"
#include "mcu/stm32f4_bsp.h"
#include "mcu/stm32f4xx_mynewt_hal.h"

#if MYNEWT_VAL(SPIFLASH)
#include <spiflash/spiflash.h>
#endif


const uint32_t stm32_flash_sectors[] = {
    0x08000000,     /* 16kB */
    0x08004000,     /* 16kB */
    0x08008000,     /* 16kB */
    0x0800c000,     /* 16kB */
    0x08010000,     /* 64kB */
    0x08020000,     /* 128kB */
    0x08040000,     /* 128kB */
    0x08060000,     /* 128kB */
    0x08080000,     /* End of flash */
};

#define SZ ARRAY_SIZE(stm32_flash_sectors)
static_assert(MYNEWT_VAL(STM32_FLASH_NUM_AREAS) + 1 == SZ,
        "STM32_FLASH_NUM_AREAS does not match flash sectors");

#if MYNEWT_VAL(TRNG)
static struct trng_dev os_bsp_trng;
#endif

#if MYNEWT_VAL(UART_0)
struct uart_dev hal_uart0;
#endif

#if MYNEWT_VAL(UART_1)
struct uart_dev hal_uart1;
#endif

#if MYNEWT_VAL(UART_2)
struct uart_dev hal_uart2;
#endif

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE) || \
    MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
#include <hal/hal_spi.h>
#endif

#if MYNEWT_VAL(ADC_1)
struct adc_dev my_dev_adc1;
#endif
#if MYNEWT_VAL(ADC_2)
struct adc_dev my_dev_adc2;
#endif
#if MYNEWT_VAL(ADC_3)
struct adc_dev my_dev_adc3;
#endif

#if MYNEWT_VAL(ADC_1)
/*
 * adc_handle is defined earlier because the DMA handle's
 * parent needs to be pointing to the adc_handle
 */
extern ADC_HandleTypeDef adc1_handle;

#define STM32F4_DEFAULT_DMA00_HANDLE {\
    .Instance = DMA2_Stream0,\
    .Init.Channel = DMA_CHANNEL_0,\
    .Init.Direction = DMA_PERIPH_TO_MEMORY,\
    .Init.PeriphInc = DMA_PINC_DISABLE,\
    .Init.MemInc = DMA_MINC_ENABLE,\
    .Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD,\
    .Init.MemDataAlignment = DMA_MDATAALIGN_WORD,\
    .Init.Mode = DMA_CIRCULAR,\
    .Init.Priority = DMA_PRIORITY_HIGH,\
    .Init.FIFOMode = DMA_FIFOMODE_DISABLE,\
    .Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL,\
    .Init.MemBurst = DMA_MBURST_SINGLE,\
    .Init.PeriphBurst = DMA_PBURST_SINGLE,\
    .Parent = &adc1_handle,\
}

DMA_HandleTypeDef adc1_dma00_handle = STM32F4_DEFAULT_DMA00_HANDLE;
#endif

#if MYNEWT_VAL(ADC_2)

extern ADC_HandleTypeDef adc2_handle;

#define STM32F4_DEFAULT_DMA21_HANDLE {\
    .Instance = DMA2_Stream2,\
    .Init.Channel = DMA_CHANNEL_1,\
    .Init.Direction = DMA_PERIPH_TO_MEMORY,\
    .Init.PeriphInc = DMA_PINC_DISABLE,\
    .Init.MemInc = DMA_MINC_ENABLE,\
    .Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD,\
    .Init.MemDataAlignment = DMA_MDATAALIGN_WORD,\
    .Init.Mode = DMA_CIRCULAR,\
    .Init.Priority = DMA_PRIORITY_HIGH,\
    .Init.FIFOMode = DMA_FIFOMODE_DISABLE,\
    .Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL,\
    .Init.MemBurst = DMA_MBURST_SINGLE,\
    .Init.PeriphBurst = DMA_PBURST_SINGLE,\
    .Parent = &adc2_handle,\
}

DMA_HandleTypeDef adc2_dma21_handle = STM32F4_DEFAULT_DMA21_HANDLE;
#endif


#if MYNEWT_VAL(ADC_3)

extern ADC_HandleTypeDef adc3_handle;

#define STM32F4_DEFAULT_DMA12_HANDLE {\
    .Instance = DMA2_Stream1,\
    .Init.Channel = DMA_CHANNEL_2,\
    .Init.Direction = DMA_PERIPH_TO_MEMORY,\
    .Init.PeriphInc = DMA_PINC_DISABLE,\
    .Init.MemInc = DMA_MINC_ENABLE,\
    .Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD,\
    .Init.MemDataAlignment = DMA_MDATAALIGN_WORD,\
    .Init.Mode = DMA_CIRCULAR,\
    .Init.Priority = DMA_PRIORITY_HIGH,\
    .Init.FIFOMode = DMA_FIFOMODE_DISABLE,\
    .Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL,\
    .Init.MemBurst = DMA_MBURST_SINGLE,\
    .Init.PeriphBurst = DMA_PBURST_SINGLE,\
    .Parent = &adc3_handle,\
}

DMA_HandleTypeDef adc3_dma12_handle = STM32F4_DEFAULT_DMA12_HANDLE;
#endif

#define STM32F4_ADC_DEFAULT_INIT_TD {\
    .ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2,\
    .Resolution = ADC_RESOLUTION12b,\
    .DataAlign = ADC_DATAALIGN_RIGHT,\
    .ScanConvMode = DISABLE,\
    .EOCSelection = DISABLE,\
    .ContinuousConvMode = ENABLE,\
    .NbrOfConversion = 1,\
    .DiscontinuousConvMode = DISABLE,\
    .NbrOfDiscConversion = 0,\
    .ExternalTrigConv = ADC_SOFTWARE_START,\
    .ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE,\
    .DMAContinuousRequests = ENABLE\
}

#if MYNEWT_VAL(ADC_1)

/*****************ADC1 Config ***************/
#define STM32F4_DEFAULT_ADC1_HANDLE {\
    .Init = STM32F4_ADC_DEFAULT_INIT_TD,\
    .Instance = ADC1,\
    .NbrOfCurrentConversionRank = 0,\
    .DMA_Handle = &adc1_dma00_handle,\
    .Lock = HAL_UNLOCKED,\
    .State = 0,\
    .ErrorCode = 0\
}

ADC_HandleTypeDef adc1_handle = STM32F4_DEFAULT_ADC1_HANDLE;

#define STM32F4_ADC1_DEFAULT_SAC {\
    .c_refmv = 3300,\
    .c_res   = 12,\
    .c_configured = 1,\
    .c_cnum = ADC_CHANNEL_10\
}

struct adc_chan_config adc1_chan10_config = STM32F4_ADC1_DEFAULT_SAC;

#define STM32F4_ADC1_DEFAULT_CONFIG {\
    .sac_chan_count = 16,\
    .sac_chans = (struct adc_chan_config [16]){{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},STM32F4_ADC1_DEFAULT_SAC},\
    .sac_adc_handle = &adc1_handle,\
}

struct stm32f4_adc_dev_cfg adc1_config = STM32F4_ADC1_DEFAULT_CONFIG;
/*********************************************/
#endif

#if MYNEWT_VAL(ADC_2)

/*****************ADC2 Config ***************/
#define STM32F4_DEFAULT_ADC2_HANDLE {\
    .Init = STM32F4_ADC_DEFAULT_INIT_TD,\
    .Instance = ADC2,\
    .NbrOfCurrentConversionRank = 0,\
    .DMA_Handle = &adc2_dma21_handle,\
    .Lock = HAL_UNLOCKED,\
    .State = 0,\
    .ErrorCode = 0\
}

ADC_HandleTypeDef adc2_handle = STM32F4_DEFAULT_ADC2_HANDLE;

#define STM32F4_ADC2_DEFAULT_SAC {\
    .c_refmv = 3300,\
    .c_res   = 12,\
    .c_configured = 1,\
    .c_cnum = ADC_CHANNEL_1\
}

struct adc_chan_config adc2_chan1_config = STM32F4_ADC2_DEFAULT_SAC;

#define STM32F4_ADC2_DEFAULT_CONFIG {\
    .sac_chan_count = 16,\
    .sac_chans = (struct adc_chan_config [16]){{0},STM32F4_ADC1_DEFAULT_SAC}\
    .sac_adc_handle = &adc2_handle,\
}

struct stm32f4_adc_dev_cfg adc2_config = STM32F4_ADC2_DEFAULT_CONFIG;
/*********************************************/
#endif

#if MYNEWT_VAL(ADC_3)

#define STM32F4_DEFAULT_ADC3_HANDLE {\
    .Init = STM32F4_ADC_DEFAULT_INIT_TD,\
    .Instance = ADC3,\
    .NbrOfCurrentConversionRank = 0,\
    .DMA_Handle = &adc3_dma12_handle,\
    .Lock = HAL_UNLOCKED,\
    .State = 0,\
    .ErrorCode = 0\
}

ADC_HandleTypeDef adc3_handle = STM32F4_DEFAULT_ADC3_HANDLE;

#define STM32F4_ADC3_DEFAULT_SAC {\
    .c_refmv = 3300,\
    .c_res   = 12,\
    .c_configured = 1,\
    .c_cnum = ADC_CHANNEL_4\
}

struct adc_chan_config adc3_chan4_config = STM32F4_ADC3_DEFAULT_SAC;

#define STM32F4_ADC3_DEFAULT_CONFIG {\
    .sac_chan_count = 16,\
    .sac_chans = (struct adc_chan_config [16]){{0},{0},{0},{0},STM32F4_ADC3_DEFAULT_SAC},\
    .sac_adc_handle = &adc3_handle,\
}

struct stm32f4_adc_dev_cfg adc3_config = STM32F4_ADC3_DEFAULT_CONFIG;
#endif

#if MYNEWT_VAL(I2C_0)
static struct stm32_hal_i2c_cfg i2c_cfg0 = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR,
    .hic_rcc_dev = RCC_APB1ENR_I2C1EN,
    .hic_pin_sda = MCU_GPIO_PORTB(9),
    .hic_pin_scl = MCU_GPIO_PORTB(8),
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_speed = 100000				/* 100kHz */
};
#endif

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
struct stm32_hal_spi_cfg spi0_cfg = {
    .ss_pin = -1,
    .sck_pin  = MYNEWT_VAL(SPI_0_SCK),
    .miso_pin = MYNEWT_VAL(SPI_0_MISO),
    .mosi_pin = MYNEWT_VAL(SPI_0_MOSI),
    .irq_prio = 2
};
#endif

#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
struct stm32_hal_spi_cfg spi0_cfg = {
    .ss_pin = -1,
    .sck_pin  = MYNEWT_VAL(SPI_1_SCK),
    .miso_pin = MYNEWT_VAL(SPI_1_MISO),
    .mosi_pin = MYNEWT_VAL(SPI_1_MOSI),
    .irq_prio = 2
};
#endif

#if MYNEWT_VAL(UART_0)
static const struct stm32_uart_cfg uart_cfg0 = {
    .suc_uart = USART1,
    .suc_rcc_reg = &RCC->APB2ENR,
    .suc_rcc_dev = RCC_APB2ENR_USART1EN,
    .suc_pin_tx = MYNEWT_VAL(UART_0_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_CTS),
    .suc_pin_af = GPIO_AF7_USART1,
    .suc_irqn = USART1_IRQn
};
#endif
#if MYNEWT_VAL(UART_1)
static const struct stm32_uart_cfg uart_cfg1 = {
    .suc_uart = USART2,
    .suc_rcc_reg = &RCC->APB2ENR,
    .suc_rcc_dev = RCC_APB1ENR_USART2EN,
    .suc_pin_tx = MYNEWT_VAL(UART_1_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_1_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_1_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_1_CTS),
    .suc_pin_af = GPIO_AF7_USART2,
    .suc_irqn = USART2_IRQn
};
#endif
#if MYNEWT_VAL(UART_2)
static const struct stm32_uart_cfg uart_cfg2 = {
    .suc_uart = USART3,
    .suc_rcc_reg = &RCC->APB2ENR,
    .suc_rcc_dev = RCC_APB1ENR_USART3EN,
    .suc_pin_tx = MYNEWT_VAL(UART_2_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_2_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_2_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_2_CTS),
    .suc_pin_af = GPIO_AF7_USART3,
    .suc_irqn = USART3_IRQn
};
#endif
#if MYNEWT_VAL(ETH_0)
static const struct stm32_eth_cfg eth_cfg = {
    /*
     * PORTA
     *   PA1 - ETH_RMII_REF_CLK
     *   PA2 - ETH_RMII_MDIO
     *   PA3 - ETH_RMII_MDINT  (GPIO irq?)
     *   PA7 - ETH_RMII_CRS_DV
     */
    .sec_port_mask[0] = (1 << 1) | (1 << 2) | (1 << 7),

    /*
     * PORTC
     *   PC1 - ETH_RMII_MDC
     *   PC4 - ETH_RMII_RXD0
     *   PC5 - ETH_RMII_RXD1
     */
    .sec_port_mask[2] = (1 << 1) | (1 << 4) | (1 << 5),

    /*
     * PORTG
     *   PG11 - ETH_RMII_TXEN
     *   PG13 - ETH_RMII_TXD0
     *   PG14 - ETH_RMII_TXD1
     */
    .sec_port_mask[6] = (1 << 11) | (1 << 13) | (1 << 14),
    .sec_phy_type = SMSC_8710_RMII,
    .sec_phy_irq = MCU_GPIO_PORTA(3)
};
#endif
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    },
    [1] = {
        .hbmd_start = &_ccram_start,
        .hbmd_size = CCRAM_SIZE
    }
};

extern const struct hal_flash stm32_flash_dev;

#if MYNEWT_VAL(SPIFLASH)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
struct bus_spi_node_cfg flash_spi_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(BSP_FLASH_SPI_BUS),
    .pin_cs = MYNEWT_VAL(SPIFLASH_SPI_CS_PIN),
    .mode = BUS_SPI_MODE_3,
    .data_order = HAL_SPI_MSB_FIRST,
    .freq = MYNEWT_VAL(SPIFLASH_BAUDRATE),
};
#endif
#endif

static const struct hal_flash *flash_devs[] = {
    [0] = &stm32_flash_dev,
#if MYNEWT_VAL(SPIFLASH)
    [1] = &spiflash_dev.hal,
#endif
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    if (id >= ARRAY_SIZE(flash_devs)) {
        return NULL;
    }

    return flash_devs[id];
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = ARRAY_SIZE(dump_cfg);
    return dump_cfg;
}

int
hal_bsp_power_state(int state)
{
    return (0);
}

/**
 * Returns the configured priority for the given interrupt. If no priority
 * configured, return the priority passed in
 *
 * @param irq_num
 * @param pri
 *
 * @return uint32_t
 */
uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    /* Add any interrupt priorities configured by the bsp here */
    return pri;
}

void
hal_bsp_init(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(TRNG)
    rc = os_dev_create(&os_bsp_trng.dev, "trng", OS_DEV_INIT_KERNEL,
                       OS_DEV_INIT_PRIO_DEFAULT, stm32_trng_dev_init, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &hal_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_cfg0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *) &hal_uart1, "uart1",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_cfg1);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_2)
    rc = os_dev_create((struct os_dev *) &hal_uart2, "uart2",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_cfg2);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(ADC_1)
    rc = os_dev_create((struct os_dev *) &my_dev_adc1, "adc1",
            OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
            stm32f4_adc_dev_init, &adc1_config);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(ADC_2)
    rc = os_dev_create((struct os_dev *) &my_dev_adc2, "adc2",
            OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
            stm32f4_adc_dev_init, &adc2_config);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(ADC_3)
    rc = os_dev_create((struct os_dev *) &my_dev_adc3, "adc3",
            OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
            stm32f4_adc_dev_init, &adc3_config);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, &i2c_cfg0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_0)
    hal_timer_init(0, TIM2);
#endif

#if MYNEWT_VAL(TIMER_1)
    hal_timer_init(1, TIM3);
#endif

#if MYNEWT_VAL(TIMER_2)
    hal_timer_init(2, TIM4);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif

#if MYNEWT_VAL(ETH_0)
    rc = stm32_eth_init(&eth_cfg);
    assert(rc == 0);
#endif
}
