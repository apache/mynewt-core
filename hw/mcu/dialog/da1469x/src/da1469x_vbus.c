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
#include <stdbool.h>
#include <stdint.h>
#include <os/util.h>
#include <mcu/da1469x_vbus.h>

/*
 * For now only two clients of VBUS notification are present USB and charger
 * If more space is needed in the future more entries can be added.
 */
static vbus_change_handler vbus_change_handlers[2];
static uint8_t vbus_change_handler_count;

static void
da1469x_vbus_isr(void)
{
    int i;
    bool vbus_present = (CRG_TOP->ANA_STATUS_REG & CRG_TOP_ANA_STATUS_REG_VBUS_AVAILABLE_Msk) != 0;

    CRG_TOP->VBUS_IRQ_CLEAR_REG = 1;
    for (i = 0; i < vbus_change_handler_count; ++i) {
        vbus_change_handlers[i](vbus_present);
    }
}

void
da1469x_vbus_add_handler(vbus_change_handler handler)
{
    int sr;
    bool vbus_present;

    assert(vbus_change_handler_count < ARRAY_SIZE(vbus_change_handlers));

    OS_ENTER_CRITICAL(sr);

    vbus_change_handlers[vbus_change_handler_count++] = handler;
    vbus_present = (CRG_TOP->ANA_STATUS_REG & CRG_TOP_ANA_STATUS_REG_VBUS_AVAILABLE_Msk) != 0;

    /* VBUS was already present notify new handler */
    if (vbus_present) {
        handler(vbus_present);
    }

    OS_EXIT_CRITICAL(sr);
}

void da1469x_vbus_init(void)
{
    NVIC_DisableIRQ(VBUS_IRQn);
    NVIC_SetVector(VBUS_IRQn, (uint32_t)da1469x_vbus_isr);
    CRG_TOP->VBUS_IRQ_CLEAR_REG = 1;
    NVIC_ClearPendingIRQ(VBUS_IRQn);
    /* Both connect and disconnect needs to be handled */
    CRG_TOP->VBUS_IRQ_MASK_REG = CRG_TOP_VBUS_IRQ_MASK_REG_VBUS_IRQ_EN_FALL_Msk |
                                 CRG_TOP_VBUS_IRQ_MASK_REG_VBUS_IRQ_EN_RISE_Msk;
    NVIC_EnableIRQ(VBUS_IRQn);
}
