/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */

#ifndef MBED_CMSIS_NVIC_H
#define MBED_CMSIS_NVIC_H

#include <stdint.h>

#if defined(STM32F746xx)
 #define MCU_NUM_PERIPH_VECTORS 98
#elif defined(STM32F767xx)
 #define MCU_NUM_PERIPH_VECTORS 110
#else
 #error "Number of peripheral vectors not defined for this MCU."
#endif

#define NVIC_USER_IRQ_OFFSET  16
#define NVIC_NUM_VECTORS      (16 + MCU_NUM_PERIPH_VECTORS)

#include "stm32f7xx.h"

#ifdef __cplusplus
extern "C" {
#endif

void NVIC_Relocate(void);
void NVIC_SetVector(IRQn_Type IRQn, uint32_t vector);
uint32_t NVIC_GetVector(IRQn_Type IRQn);

#ifdef __cplusplus
}
#endif

#endif
