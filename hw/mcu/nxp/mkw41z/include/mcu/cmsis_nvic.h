/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */
#ifndef H_CMSIS_NVIC_
#define H_CMSIS_NVIC_

#include <stdint.h>

#include "mcu/MKW41Z4.h"

/* The MKW41Z has 32 user interrupt vectors */
#define NVIC_USER_IRQ_OFFSET  16
#define NVIC_NUM_VECTORS      (NVIC_USER_IRQ_OFFSET + 32)

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
