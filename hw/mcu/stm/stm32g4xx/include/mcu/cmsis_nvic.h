/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */

#ifndef MCU_CMSIS_NVIC_H
#define MCU_CMSIS_NVIC_H

#include <stdint.h>

extern uint32_t __isr_vector_start[];
extern uint32_t __isr_vector_end[];

/* Extract number of vectors from .interrupt section size */
#define NVIC_NUM_VECTORS      (__isr_vector_end - __isr_vector_start)
#define NVIC_USER_IRQ_OFFSET  16

#include <stm32g4xx.h>

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
