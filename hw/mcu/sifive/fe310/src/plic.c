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

#include <env/freedom-e300-hifive1/platform.h>
#include <mcu/plic.h>
#include <syscfg/syscfg.h>

interrupt_handler_t plic_interrupts[PLIC_NUM_INTERRUPTS];

void
plic_set_handler(int int_num, interrupt_handler_t handler, int priority)
{
    if (int_num > 0 && int_num < PLIC_NUM_INTERRUPTS) {
        plic_interrupts[int_num] = handler;
        PLIC_REG(PLIC_PRIORITY_OFFSET + int_num * 4) = priority;
    }
}

void
plic_enable_interrupt(int int_num)
{
    if (int_num > 0 && int_num < PLIC_NUM_INTERRUPTS) {
        PLIC_REG(PLIC_ENABLE_OFFSET + 4 * (int_num / 32)) |= 1 << (int_num & 0x1F);
    }
}

void
plic_disable_interrupt(int int_num)
{
    if (int_num > 0 && int_num < PLIC_NUM_INTERRUPTS) {
        PLIC_REG(PLIC_ENABLE_OFFSET + 4 * (int_num / 32)) &= ~(1 << (int_num & 0x1F));
    }
}

void
external_interrupt_handler(uintptr_t mcause)
{
    int num = PLIC_REG(PLIC_CLAIM_OFFSET);
    /*
     * Interrupts have some overhead, handle all pending interrupts.
     */
    while (num) {
        /* Confirm interrupt*/
        PLIC_REG(PLIC_CLAIM_OFFSET) = num;
        /* Call interrupt handler */
        plic_interrupts[num](num);
        /* Check if other interupt is already pending */
        num = PLIC_REG(PLIC_CLAIM_OFFSET);
    }
}
