/**
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

#include "mcu/gic.h"

#include <mips/cpu.h>
#include <mips/hal.h>

#include <string.h>

static const uint32_t GIC_BASE = 0x1bdc0000ul;

static const uint32_t GIC_EN = 1;
static const uint32_t GIC_MAP_TO_PIN = 1 << 31;

static uint32_t* _GCR = NULL;
static uint32_t* _GIC = NULL;

static inline void
gic_enable(void)
{
    _GCR[0x80 / sizeof(uint32_t)] |= GIC_EN;
}

static inline void
gic_disable(void)
{
    _GCR[0x80 / sizeof(uint32_t)] &= ~GIC_EN;
}

static uint32_t*
gic_sh_map_vpe(uint32_t n)
{
    return &_GIC[(0x2000 + (n * 0x0020)) / sizeof(uint32_t)];
}

static uint32_t*
gic_sh_map_pin(uint32_t n)
{
    return &_GIC[(0x0500 + (n * 0x0004)) / sizeof(uint32_t)];
}

void
gic_interrupt_set(uint32_t n)
{
    _GIC[(0x380 / sizeof(uint32_t)) + (n / 32)] = 1ul << (n % 32);
}

void
gic_interrupt_reset(uint32_t n)
{
    _GIC[(0x300 / sizeof(uint32_t)) + (n / 32)] = 1ul << (n % 32);
}

void
gic_interrupt_active_high(uint32_t n)
{
    _GIC[(0x100 / sizeof(uint32_t)) + (n / 32)] = 1ul << (n % 32);
}

void
gic_interrupt_active_low(uint32_t n)
{
    _GIC[(0x100 / sizeof(uint32_t)) + (n / 32)] &= ~(1ul << (n % 32));
}

int
gic_interrupt_is_enabled(uint32_t n)
{
    return !!(_GIC[(0x400 / sizeof(uint32_t)) + (n / 32)] & (1ul << (n % 32)));
}

int
gic_interrupt_poll(uint32_t n)
{
    return !!(_GIC[(0x480 / sizeof(uint32_t)) + (n / 32)] & (1ul << (n % 32)));
}

void
gic_map(int int_no, uint8_t vpe, uint8_t pin)
{
    /* map UART0 to HW interrupt 0 */
    *gic_sh_map_vpe(int_no) = 1 << vpe;
    *gic_sh_map_pin(int_no) = GIC_MAP_TO_PIN | pin;
    /* enable interrupt in status register */
    mips_bissr(0x400 << pin);
}

void
gic_unmap(int int_no, uint8_t pin)
{
    /* unmap UART0 from HW interrupt 0 */
    *gic_sh_map_vpe(int_no) = 0;
    *gic_sh_map_pin(int_no) = 0;
    /* disable interrupt in status register */
    mips_bicsr(0x400 << pin);
}

static void
gic_place(uint32_t base)
{
    base &= ~GIC_EN;
    _GCR[0x80 / sizeof(uint32_t)] &= GIC_EN;
    _GCR[0x80 / sizeof(uint32_t)] |= base;
    _GIC = PA_TO_KVA1(base);
}

int
gic_init(void)
{
    /* Check for GCR */
    if (!((mips32_getconfig0() & CFG0_M) && (mips32_getconfig1() & CFG1_M)
        && (mips32_getconfig2() & CFG2_M)
        && (mips32_getconfig3() & CFG3_CMGCR))) {
        return -1;
    }

    /* get GCR base address */
    _GCR = PA_TO_KVA1((mips32_get_c0(C0_CMGCRBASE) & 0x0ffffc00) << 4);
    gic_place(GIC_BASE);
    gic_enable();

    return 0;
}
