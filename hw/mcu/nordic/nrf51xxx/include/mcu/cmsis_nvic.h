/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */

#ifndef MBED_CMSIS_NVIC_H
#define MBED_CMSIS_NVIC_H

#include <stdint.h>

/* NOTE: the nrf51 SoC has 26 interrupts. */
#define NVIC_USER_IRQ_OFFSET  16
#define NVIC_NUM_VECTORS      (NVIC_USER_IRQ_OFFSET + 26)

#include "nrf51.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
