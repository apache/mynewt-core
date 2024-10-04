/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */

#ifndef CMSIS_NVIC_H
#define CMSIS_NVIC_H

#include <stdint.h>
#include "syscfg/syscfg.h"

#define NVIC_NUM_VECTORS      (16 + 107)   // CORE + MCU Peripherals
#define NVIC_USER_IRQ_OFFSET  16

/* Helper functions to enable/disable interrupts. */
#define __HAL_DISABLE_INTERRUPTS(x)             \
    do {                                        \
        x = __get_PRIMASK();                    \
        __disable_irq();                        \
    } while(0);

#define __HAL_ENABLE_INTERRUPTS(x)              \
    do {                                        \
        if (!x) {                               \
            __enable_irq();                     \
        }                                       \
    } while(0);

/*
 * include board definition file which includes: cmsis-core/core_cm4.h
 * this fixes missing CORTEX_M* definition in cmsis_nvic.c
 */
#include <fsl_device_registers.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
