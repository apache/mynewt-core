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
 *   @defgroup HALNvreg HAL NVReg
 *   @{
 */

#ifndef __HAL_NVREG_H_
#define __HAL_NVREG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/**
 * Writes to a retained register
 */
void hal_nvreg_write(unsigned int reg, uint32_t val);

/**
 * Reads from a retained register
 */
uint32_t hal_nvreg_read(unsigned int reg);

/**
 * Get number of available retained registers
 */
unsigned int hal_nvreg_get_num_regs(void);

/**
 * Get retained register width (in bytes)
 */
unsigned int hal_nvreg_get_reg_width(void);

#ifdef __cplusplus
}
#endif

#endif

/**
 *   @} HALNvreg
 * @} HAL
 */
