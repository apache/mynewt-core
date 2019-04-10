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

#if MYNEWT_VAL(UART_0)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#include <hal/hal_bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_timer.h>

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE) || \
    MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
#include <hal/hal_spi.h>
#endif

#include <stm32f103xb.h>
#include <stm32f1xx_hal_rcc.h>
#include <stm32f1xx_hal_pwr.h>
#include <stm32f1xx_hal_flash.h>
#include <stm32f1xx_hal_gpio_ex.h>
#include <stm32f1xx_ll_gpio.h>       /* AF remapping */
#include <mcu/stm32f1_bsp.h>
#include "mcu/stm32f1xx_mynewt_hal.h"
#include "mcu/stm32_hal.h"
#include "hal/hal_i2c.h"

#if MYNEWT_VAL(SPIFLASH)
#include <spiflash/spiflash.h>
#endif

#include "bsp/bsp.h"

#if UART_CNT > 0
static struct uart_dev hal_uart[UART_CNT];

static const struct stm32_uart_cfg uart_cfg[UART_CNT] = {
#if MYNEWT_VAL(UART_0)
    {
        .suc_uart         = USART2,
        .suc_rcc_reg      = &RCC->APB1ENR,
        .suc_rcc_dev      = RCC_APB1ENR_USART2EN,
        .suc_pin_tx       = MCU_GPIO_PORTA(2),
        .suc_pin_rx       = MCU_GPIO_PORTA(3),
        .suc_pin_rts      = -1,
        .suc_pin_cts      = -1,
        .suc_pin_remap_fn = LL_GPIO_AF_DisableRemap_USART2,
        .suc_irqn         = USART2_IRQn
    },
#endif
#if MYNEWT_VAL(UART_1)
{
        .suc_uart         = USART1,
        .suc_rcc_reg      = &RCC->APB2ENR,
        .suc_rcc_dev      = RCC_APB2ENR_USART1EN,
        .suc_pin_tx       = MCU_GPIO_PORTA(9),
        .suc_pin_rx       = MCU_GPIO_PORTA(10),
        .suc_pin_rts      = -1,
        .suc_pin_cts      = -1,
        .suc_pin_remap_fn = LL_GPIO_AF_DisableRemap_USART1,
        .suc_irqn         = USART1_IRQn
    },
#endif
#if MYNEWT_VAL(UART_2)
{
        .suc_uart         = USART3,
        .suc_rcc_reg      = &RCC->APB1ENR,
        .suc_rcc_dev      = RCC_APB1ENR_USART3EN,
        .suc_pin_tx       = MCU_GPIO_PORTB(10),
        .suc_pin_rx       = MCU_GPIO_PORTB(11),
        .suc_pin_rts      = -1,
        .suc_pin_cts      = -1,
        .suc_pin_remap_fn = LL_GPIO_AF_DisableRemap_USART3,
        .suc_irqn         = USART3_IRQn
    },
#endif
};

const struct stm32_uart_cfg *
bsp_uart_config(int port)
{
    assert(port < UART_CNT);
    return &uart_cfg[port];
}

static const char *uart_dev_name[UART_CNT] = {
#if UART_CNT > 0
    "uart0",
#endif
#if UART_CNT > 1
    "uart1",
#endif
#if UART_CNT > 2
    "uart2",
#endif
};

#endif

/* NOTE: UEXT connector */
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
struct stm32_hal_spi_cfg spi0_cfg = {
    .ss_pin   = MCU_GPIO_PORTA(4),
    .sck_pin  = MCU_GPIO_PORTA(5),
    .miso_pin = MCU_GPIO_PORTA(6),
    .mosi_pin = MCU_GPIO_PORTA(7),
    .irq_prio = 2,
};
#endif

/* NOTE: SD-MMC */
#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
struct stm32_hal_spi_cfg spi1_cfg = {
    .ss_pin   = MCU_GPIO_PORTB(12),
    .sck_pin  = MCU_GPIO_PORTB(13),
    .miso_pin = MCU_GPIO_PORTB(14),
    .mosi_pin = MCU_GPIO_PORTB(15),
    .irq_prio = 2,
};
#endif

#if MYNEWT_VAL(I2C_0)
static struct stm32_hal_i2c_cfg i2c_cfg0 = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR,
    .hic_rcc_dev = RCC_APB1ENR_I2C1EN,
    .hic_pin_sda = MCU_GPIO_PORTB(7),
    .hic_pin_scl = MCU_GPIO_PORTB(6),
    .hic_pin_remap_fn = LL_GPIO_AF_DisableRemap_I2C1,
    .hic_10bit = 0,
    .hic_speed = 100000,
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    },
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
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

static void
clock_config(void)
{
    RCC_ClkInitTypeDef clkinitstruct = { 0 };
    RCC_OscInitTypeDef oscinitstruct = { 0 };

    /* Configure PLL ------------------------------------------------------*/
    /* PLL configuration: PLLCLK = (HSI / 2) * PLLMUL = (8 / 2) * 16 = 64 MHz */
    /* PREDIV1 configuration: PREDIV1CLK = PLLCLK / HSEPredivValue = 64 / 1 = 64 MHz */
    /* Enable HSI and activate PLL with HSi_DIV2 as source */
    oscinitstruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    oscinitstruct.HSEState = RCC_HSE_OFF;
    oscinitstruct.LSEState = RCC_LSE_OFF;
    oscinitstruct.HSIState = RCC_HSI_ON;
    oscinitstruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    oscinitstruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    oscinitstruct.PLL.PLLState = RCC_PLL_ON;
    oscinitstruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    oscinitstruct.PLL.PLLMUL = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&oscinitstruct) != HAL_OK) {
        assert(0);
    }

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
       clocks dividers */
    clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                               RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV4;
    clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2) != HAL_OK) {
        assert(0);
    }
}

void
hal_bsp_init(void)
{
    int rc;

    (void)rc;

    clock_config();

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *)&hal_uart[UART_0_DEV_ID],
                       uart_dev_name[UART_0_DEV_ID], OS_DEV_INIT_PRIMARY, 0,
                       uart_hal_init, (void *)&uart_cfg[UART_0_DEV_ID]);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *)&hal_uart[UART_1_DEV_ID],
                       uart_dev_name[UART_1_DEV_ID], OS_DEV_INIT_PRIMARY, 0,
                       uart_hal_init, (void *)&uart_cfg[UART_1_DEV_ID]);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_2)
    rc = os_dev_create((struct os_dev *)&hal_uart[UART_2_DEV_ID],
                       uart_dev_name[UART_2_DEV_ID], OS_DEV_INIT_PRIMARY, 0,
                       uart_hal_init, (void *)&uart_cfg[UART_2_DEV_ID]);
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

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_1_MASTER)
    rc = hal_spi_init(1, &spi1_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_1_SLAVE)
    rc = hal_spi_init(1, &spi1_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, &i2c_cfg0);
    assert(rc == 0);
#endif

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
