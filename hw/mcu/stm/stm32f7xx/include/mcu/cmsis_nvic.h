/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */

#ifndef MBED_CMSIS_NVIC_H
#define MBED_CMSIS_NVIC_H

#include <stdint.h>
#include <syscfg/syscfg.h>

#if MYNEWT_VAL(MCU_NVIC_PERIPH_VECTORS) < 16
#error "Too few peripheral IRQ vectors, incorrect config?"
#endif

#define NVIC_USER_IRQ_OFFSET  16
#define NVIC_NUM_VECTORS      (16 + MYNEWT_VAL(MCU_NVIC_PERIPH_VECTORS))

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
