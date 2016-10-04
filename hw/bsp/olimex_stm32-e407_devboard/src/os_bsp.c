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

/*
 * XXXX for now have this here.
 */
#include <assert.h>

#include <flash_map/flash_map.h>
#include <hal/hal_bsp.h>

#include <os/os_dev.h>
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>

#include "bsp/bsp.h"
#include "syscfg/syscfg.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_adc.h"
#include <adc_stm32f4/adc_stm32f4.h>

static struct uart_dev hal_uart0;

/* XXX should not be here */

#if MYNEWT_VAL(ADC_1)
struct adc_dev my_dev_adc1;
#endif
#if MYNEWT_VAL(ADC_2)
struct adc_dev my_dev_adc2;
#endif
#if MYNEWT_VAL(ADC_3)
struct adc_dev my_dev_adc3;
#endif

struct stm32f4_uart_cfg;

extern struct stm32f4_uart_cfg *bsp_uart_config(int port);

/*****************ADC3 Config ***************/

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

void
bsp_init(void)
{
    int rc;

    rc = os_dev_create((struct os_dev *) &hal_uart0, CONSOLE_UART,
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)bsp_uart_config(0));
    assert(rc == 0);
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

}

