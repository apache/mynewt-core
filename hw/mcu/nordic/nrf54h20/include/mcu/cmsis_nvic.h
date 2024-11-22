/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */

#ifndef _CMSIS_NVIC_H_
#define _CMSIS_NVIC_H_

#include <stdint.h>
#include <nrf.h>

#define NVIC_NUM_VECTORS      (16 + 30)   /* CORE + MCU Peripherals */
#define NVIC_USER_IRQ_OFFSET  16

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
