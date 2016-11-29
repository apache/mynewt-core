/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */

#ifndef CMSIS_NVIC_H
#define CMSIS_NVIC_H

#include <stdint.h>

#define NVIC_NUM_VECTORS      (16 + 101)   // CORE + MCU Peripherals
#define NVIC_USER_IRQ_OFFSET  16

/*
 * include board definition file which includes: cmsis-core/core_cm4.h
 * this fixes missing CORTEX_M* definition in cmsis_nvic.c
 */
#include "MK64F12.h"

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
