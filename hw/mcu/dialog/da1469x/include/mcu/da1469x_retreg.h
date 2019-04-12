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

#ifndef __MCU_DA1469X_RETREG_H_
#define __MCU_DA1469X_RETREG_H_

#include <stdint.h>
#include "DA1469xAB.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dummy register address to fill empty slots. This register is in fact read
 * only and writes are ignored so are basically harmless.
 */
#define MCU_RETREG_ADDR_DUMMY       (&CHIP_VERSION->CHIP_ID1_REG)

/** Retained register container (a.k.a. retreg) */
struct da1469x_retreg {
    volatile uint32_t *reg;
    uint32_t value;
};

/**
 * Initialize set of retregs
 *
 * This invalidates each retreg.
 *
 * @param retregs  Retregs array
 * @param count    Number of elements in set
 */
void da1469x_retreg_init(struct da1469x_retreg *retregs, uint8_t count);

/**
 * Update set of retregs
 *
 * This updates stored value of each retreg with current value of corresponding
 * register.
 *
 * @param retregs  Retregs array
 * @param count    Number of elements in set
 */
void da1469x_retreg_update(struct da1469x_retreg *retregs, uint8_t count);

/**
 * Restore set of retregs
 *
 * This sets corresponding register value of each retreg to stored value.
 *
 * @param retregs  Retregs array
 * @param count    Number of elements in set
 */
void da1469x_retreg_restore(struct da1469x_retreg *retregs, uint8_t count);

/**
 * Invalidate retreg entry
 *
 * @param retreg  Retreg entry
 */
static inline void
da1469x_retreg_invalidate(struct da1469x_retreg *retreg)
{
    retreg->reg = MCU_RETREG_ADDR_DUMMY;
    retreg->value = 0;
}

/**
 * Assign register to retreg
 *
 * This assigns specified register to retreg and stores its current value.
 *
 * @param retreg  Retreg entry
 * @param reg     Register to assign
 */
static inline void
da1469x_retreg_assign(struct da1469x_retreg *retreg, volatile uint32_t *reg)
{
    retreg->value = *reg;
    retreg->reg = reg;
}

#ifdef __cplusplus
}
#endif

#endif /* __MCU_DA1469X_RETREG_H_ */
