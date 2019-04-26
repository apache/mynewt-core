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

#ifndef __MCU_DA1469X_PDC_H_
#define __MCU_DA1469X_PDC_H_

#include <stdbool.h>
#include <stdint.h>
#include "DA1469xAB.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MCU_PDC_CTRL_REGS(_i)       (((__IO uint32_t*)&PDC->PDC_CTRL0_REG)[_i])
#define MCU_PDC_CTRL_REGS_COUNT     16

/* PDC trigger can be either a GPIO number or one of following values */
#define MCU_PDC_TRIGGER_TIMER               (0x40 | 0)
#define MCU_PDC_TRIGGER_TIMER2              (0x40 | 1)
#define MCU_PDC_TRIGGER_TIMER3              (0x40 | 2)
#define MCU_PDC_TRIGGER_TIMER4              (0x40 | 3)
#define MCU_PDC_TRIGGER_RTC_ALARM           (0x40 | 4)
#define MCU_PDC_TRIGGER_RTC_TIMER           (0x40 | 5)
#define MCU_PDC_TRIGGER_MAC_TIMER           (0x40 | 6)
#define MCU_PDC_TRIGGER_MOTOR_CONTROLLER    (0x40 | 7)
#define MCU_PDC_TRIGGER_XTAL32M_READY       (0x40 | 8)
#define MCU_PDC_TRIGGER_RFDIAG              (0x40 | 9)
#define MCU_PDC_TRIGGER_COMBO               (0x40 | 10) /* VBUS, IO, JTAG, CMAC2SYS */
#define MCU_PDC_TRIGGER_SNC                 (0x40 | 11)
#define MCU_PDC_TRIGGER_SW_TRIGGER          (0x40 | 15)

/* PDC master can be either of following values */
#define MCU_PDC_MASTER_M33                  1
#define MCU_PDC_MASTER_CMAC                 2
#define MCU_PDC_MASTER_SNC                  3

/* PDC enable bitmask can consist of following values */
#define MCU_PDC_EN_NONE                     0x00
#define MCU_PDC_EN_XTAL                     0x01
#define MCU_PDC_EN_PD_TMR                   0x02
#define MCU_PDC_EN_PD_PER                   0x04
#define MCU_PDC_EN_PD_COM                   0x08

/**
 * Add entry to PDC lookup table
 *
 * This adds new entry to PDC lookup table. Unused entry index is selected
 * automatically.
 *
 * @param trigger  Trigger source
 * @param master   Master to wakeup
 * @param en       Power domains to enable
 *
 * @return entry index, SYS_ENOENT if all lookup table entries are used
 */
int da1469x_pdc_add(uint8_t trigger, uint8_t master, uint8_t en);

/**
 * Delete entry from PDC lookup table
 *
 * This removed existing entry from PDC lookup table. It assumes requested
 * entry is set.
 *
 * @param idx  Entry index
 */
void da1469x_pdc_del(int idx);

/**
 * Find entry in PDC lookup table matching given values
 *
 * Set either \p trigger or \p master to negative value to disable matching on
 * that value.
 * \p en matches at least specified power domains, more domains can be included
 * in matched entry.
 *
 * @param trigger  Trigger to match
 * @param master   Master to wakeup to match
 * @param en       Required power domains to enable
 *
 * @return entry index on success, SYS_ENOENT if not found
 */
int da1469x_pdc_find(int trigger, int master, uint8_t en);

/**
 * Reset PDC lookup table
 *
 * This deletes all valid entried from LUT and acknowledges them in case some
 * were pending.
 */
void da1469x_pdc_reset(void);

/**
 * Acknowledge pending PDC lookup table entry
 *
 * @param idx  Entry index
 */
static inline void
da1469x_pdc_ack(int idx)
{
    PDC->PDC_ACKNOWLEDGE_REG = idx;
}

/**
 * Set PDC lookup table entry as pending
 *
 * @param idx  Entry index
 */
static inline void
da1469x_pdc_set(int idx)
{
    PDC->PDC_SET_PENDING_REG = idx;
}

/**
 * Check if PDC lookup table entry is pending
 *
 * @param idx  Entry index
 *
 * @return true if entry is pending, false otherwise
 */
static inline bool
da1469x_pdc_is_pending(int idx)
{
    return PDC->PDC_PENDING_REG & (1 << idx);
}

/**
 * Acknowledge all pending entries on M33 core
 */
void da1469x_pdc_ack_all_m33(void);

#ifdef __cplusplus
}
#endif

#endif /* __MCU_DA1469X_PDC_H_ */
