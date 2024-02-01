/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */

#ifndef MCU_CMSIS_NVIC_H
#define MCU_CMSIS_NVIC_H

#include <stdint.h>

#define NVIC_USER_IRQ_OFFSET  16
#define NVIC_VECTOR_
enum {
//#include <mcu/mcu_vectors.h>
    NVIC_VECTOR_COUNT = 31 + 16
};
#define NVIC_NUM_VECTORS      (int)NVIC_VECTOR_COUNT

#include <stm32g0xx.h>

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
