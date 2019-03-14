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

#include <assert.h>
#include "defs/error.h"
#include "mcu/da1469x_pdc.h"

int
da1469x_pdc_add(uint8_t source, uint8_t master, uint8_t en)
{
    int idx;
    uint8_t select;

    select = source >> 5;
    source &= 0x1f;

    for (idx = 0; idx < MCU_PDC_CTRL_REGS_COUNT; idx++) {
        if (!(MCU_PDC_CTRL_REGS(idx) & PDC_PDC_CTRL0_REG_PDC_MASTER_Msk)) {
            MCU_PDC_CTRL_REGS(idx) = (select << PDC_PDC_CTRL0_REG_TRIG_SELECT_Pos) |
                                 (source << PDC_PDC_CTRL0_REG_TRIG_ID_Pos) |
                                 (master << PDC_PDC_CTRL0_REG_PDC_MASTER_Pos) |
                                 (en << PDC_PDC_CTRL0_REG_EN_XTAL_Pos);
            return idx;
        }
    }

    assert(0);

    return SYS_ENOENT;
}

void
da1469x_pdc_del(int idx)
{
    assert((idx >= 0) && (idx < MCU_PDC_CTRL_REGS_COUNT));
    assert(MCU_PDC_CTRL_REGS(idx) & PDC_PDC_CTRL0_REG_PDC_MASTER_Msk);

    MCU_PDC_CTRL_REGS(idx) &= ~PDC_PDC_CTRL0_REG_PDC_MASTER_Msk;
}

int
da1469x_pdc_find(int trigger, int master, uint8_t en)
{
    int idx;
    uint32_t mask;
    uint32_t value;

    mask = en << PDC_PDC_CTRL0_REG_EN_XTAL_Pos;
    value = en << PDC_PDC_CTRL0_REG_EN_XTAL_Pos;
    if (trigger >= 0) {
        mask |= PDC_PDC_CTRL0_REG_TRIG_SELECT_Msk | PDC_PDC_CTRL0_REG_TRIG_ID_Msk;
        value |= ((trigger >> 5) << PDC_PDC_CTRL0_REG_TRIG_SELECT_Pos) |
                 ((trigger & 0x1f) << PDC_PDC_CTRL0_REG_TRIG_ID_Pos);
    }
    if (master > 0) {
        mask |= PDC_PDC_CTRL0_REG_PDC_MASTER_Msk;
        value |= master << PDC_PDC_CTRL0_REG_PDC_MASTER_Pos;
    }
    assert(mask);

    for (idx = 0; idx < MCU_PDC_CTRL_REGS_COUNT; idx++) {
        if ((MCU_PDC_CTRL_REGS(idx) & mask) == value) {
            return idx;
        }
    }

    return SYS_ENOENT;
}

void
da1469x_pdc_reset(void)
{
    int idx;

    for (idx = 0; idx < MCU_PDC_CTRL_REGS_COUNT; idx++) {
        MCU_PDC_CTRL_REGS(idx) = 0;
        da1469x_pdc_ack(idx);
    }
}

void
da1469x_pdc_ack_all_m33(void)
{
    int idx;

    for (idx = 0; idx < MCU_PDC_CTRL_REGS_COUNT; idx++) {
        if (PDC->PDC_PENDING_CM33_REG & (1 << idx)) {
            da1469x_pdc_ack(idx);
        }
    }
}
