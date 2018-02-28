/*
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


/**
 * @addtogroup HAL
 * @{
 *   @defgroup HALBsp HAL BSP
 *   @{
 */

#ifndef __HAL_BSP_H_
#define __HAL_BSP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/**
 * Initializes BSP; registers flash_map with the system.
 */
void hal_bsp_init(void);

/**
 * Return pointer to flash device structure, given BSP specific
 * flash id.
 */
struct hal_flash;
const struct hal_flash *hal_bsp_flash_dev(uint8_t flash_id);

/*
 * Grows heap by given amount. XXX giving space back not implemented.
 */
void *_sbrk(int incr);

struct hal_bsp_mem_dump {
    void *hbmd_start;
    uint32_t hbmd_size;
};

/**
 * Report which memory areas should be included inside a coredump.
 */
const struct hal_bsp_mem_dump *hal_bsp_core_dump(int *area_cnt);

#define HAL_BSP_MAX_ID_LEN  32
/**
 * Get unique HW identifier/serial number for platform.
 * Returns the number of bytes filled in.
 *
 * @param id      Pointer to the ID to fill out
 * @param max_len Maximum length to fill into the ID
 *
 * @return 0 on success, non-zero error code on failure
 */
int hal_bsp_hw_id(uint8_t *id, int max_len);

/** Full System On */
#define HAL_BSP_POWER_ON (1)
/** Wait for Interrupt: CPU off */
#define HAL_BSP_POWER_WFI (2)
/** System sleep mode, processor off, some peripherals off too */
#define HAL_BSP_POWER_SLEEP (3)
/**
 * Deep sleep: possible loss of RAM retention, system wakes up in
 * undefined state.
 */
#define HAL_BSP_POWER_DEEP_SLEEP (4)
/**
 * System powering off
 */
#define HAL_BSP_POWER_OFF (5)
/**
 * Per-user power state, base number for user to define custom power states.
 */
#define HAL_BSP_POWER_PERUSER (128)

/**
 * Move the system into the specified power state
 *
 * @param state The power state to move the system into, this is one of
 *                 * HAL_BSP_POWER_ON: Full system on
 *                 * HAL_BSP_POWER_WFI: Processor off, wait for interrupt.
 *                 * HAL_BSP_POWER_SLEEP: Put the system to sleep
 *                 * HAL_BSP_POWER_DEEP_SLEEP: Put the system into deep sleep.
 *                 * HAL_BSP_POWER_OFF: Turn off the system.
 *                 * HAL_BSP_POWER_PERUSER: From this value on, allow user
 *                   defined power states.
 *
 * @return 0 on success, non-zero if system cannot move into this power state.
 */
int hal_bsp_power_state(int state);

/**
 * Returns priority of given interrupt number
 */
uint32_t hal_bsp_get_nvic_priority(int irq_num, uint32_t pri);

#ifdef __cplusplus
}
#endif

#endif


/**
 *   @} HALBsp
 * @} HAL
 */
