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

#include "mcu/mcu.h"
#include "hal/hal_os_tick.h"
#include "CMAC.h"

/*
 * Interrupts handling on CMAC needs to be a bit different than in generic M0+
 * arch. We need FIELD, FRAME and CALLBACK interrupts to be handled in real-time
 * as otherwise PHY will fail due to interrupt latency. For this reason we need
 * to have BASEPRI-like behavior but since M0+ does not support BASEPRI in hw,
 * this is emulated in sw.
 *
 * CMAC.h includes os_arm_cmac.h which redefines NVIC_Enable/Disable to make
 * sure ICER/ISER is not accessed directly outside this file.
 *
 * Configured state for ISER is kept in shadow register (g_cmac_iser_shadow)
 * and is applied only when not in critical section or when exiting critical
 * section.
 *
 * When entering critical section all standard (i.e. other than those we need
 * to keep always enabled) interrupts are disabled via ICER and flag is set to
 * mimic PRIMASK behavior.
 *
 * Additional handling is required for PendSV since it cannot be disabled from
 * NVIC so there is another flag which indicates that PendSV should be set. This
 * is done when exiting critical section - we assume that PendSV is being set to
 * pending only when in critical section (which is true for Mynewt since it's
 * done on context switch and we do not expect our controller on CMAC to work
 * with any other OS ever).
 *
 * Finally, before execuing 'wfi' we need to globally disable all interrupts but
 * so we can restore ISER for all interrupts and thus 'wfi' will wakeup on any
 * configured interrupt. After 'wfi', ICER is used to restore pre-wfi state and
 * interrupts and globally unlocked.
 */

/* XXX temporary, for dev only */
#define CMAC_ARCH_SANITY_CHECK      (1)

/*
 * There are 3 groups of interrupts:
 * - BS_CTRL -> bistream controller (FIELD/FRAME/CALLBACK) interrupts which
 *              are not disabled while in critical section and can be only
 *              disabled explicitly using NVIC_DisableIRQ or blocked using
 *              os_arch_cmac_bs_ctrl_irq_block()
 *              these interrupts cannot call functions that access OS or other
 *              shared data
 * - HI_PRIO -> SW_MAC interrupt which is disabled in critical section like
 *              other interrupts, but will be enabled when executing idle
 *              entry/exit code so it is not affected by increased latency
 *              introduced by that code
 *              this interrupt can safely access OS and other shared data,
 *              except for functions that need accurate os_time since system
 *              tick may be not up to date
 * - OTHER   -> all other interrupts
 */

/*
 * XXX for now add crypto to bs_ctrl although it probably should have separate
 *     group since it's perfectly fine for it to be enabled during handover
 */
#define CMAC_ARCH_I_BS_CTRL         (0x0107)
#define CMAC_ARCH_I_HI_PRIO         (0x0020)
#define CMAC_ARCH_I_OTHER           (0x1ed8)
#define CMAC_ARCH_I_NON_BS_CTRL     (CMAC_ARCH_I_HI_PRIO | \
                                     CMAC_ARCH_I_OTHER)
#define CMAC_ARCH_I_ALL             (CMAC_ARCH_I_BS_CTRL | \
                                     CMAC_ARCH_I_HI_PRIO | \
                                     CMAC_ARCH_I_OTHER)
#define CMAC_ARCH_F_PRIMASK         (0x0001)
#define CMAC_ARCH_F_PENDSV          (0x0002)
#define CMAC_ARCH_F_IDLE_SECTION    (0x0004)
#define CMAC_ARCH_F_BS_CTRL_BLOCKED (0x0008)

static uint16_t g_cmac_arch_flags;
static uint16_t g_cmac_arch_iser_shadow;

extern int cmac_sleep_do_sleep(void) __attribute__((naked));

os_sr_t
os_arch_save_sr(void)
{
    uint32_t ctx;

    __disable_irq();

    ctx = g_cmac_arch_flags & CMAC_ARCH_F_PRIMASK;

#if CMAC_ARCH_SANITY_CHECK
    assert(!ctx ||
           (!(g_cmac_arch_flags & CMAC_ARCH_F_IDLE_SECTION) && !(NVIC->ISER[0] & CMAC_ARCH_I_NON_BS_CTRL)) ||
           ((g_cmac_arch_flags & CMAC_ARCH_F_IDLE_SECTION) && !(NVIC->ISER[0] & CMAC_ARCH_I_OTHER)));
#endif

    if (g_cmac_arch_flags & CMAC_ARCH_F_IDLE_SECTION) {
        NVIC->ICER[0] = CMAC_ARCH_I_OTHER;
    } else {
        NVIC->ICER[0] = CMAC_ARCH_I_NON_BS_CTRL;
    }
    g_cmac_arch_flags |= CMAC_ARCH_F_PRIMASK;

    __enable_irq();

    return ctx;
}

void
os_arch_restore_sr(os_sr_t ctx)
{
    if (ctx) {
        return;
    }

#if CMAC_ARCH_SANITY_CHECK
    assert(g_cmac_arch_flags & CMAC_ARCH_F_PRIMASK);
    assert(!(NVIC->ISER[0] & CMAC_ARCH_I_NON_BS_CTRL));
#endif

    __disable_irq();

    if (g_cmac_arch_flags & CMAC_ARCH_F_PENDSV) {
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
    g_cmac_arch_flags &= ~(CMAC_ARCH_F_PRIMASK | CMAC_ARCH_F_PENDSV);

    NVIC->ISER[0] = g_cmac_arch_iser_shadow & CMAC_ARCH_I_NON_BS_CTRL;

    __enable_irq();
}

int
os_arch_in_critical(void)
{
    return g_cmac_arch_flags & CMAC_ARCH_F_PRIMASK;
}

void
os_arch_cmac_enable_irq(IRQn_Type irqn)
{
    uint32_t irqm = 1 << irqn;

    __disable_irq();

    g_cmac_arch_iser_shadow |= irqm;

    /*
     * Enable interrupt in NVIC if either:
     * - we are not in critical section
     * - this is BS_CTRL interrupt and they are not blocked right now
     */
    if (!os_arch_in_critical() ||
        ((irqm & CMAC_ARCH_I_BS_CTRL) &&
         !(g_cmac_arch_flags & CMAC_ARCH_F_BS_CTRL_BLOCKED))) {
        NVIC->ISER[0] = irqm;
    }

    __enable_irq();
}

uint32_t
os_arch_cmac_get_enable_irq(IRQn_Type irqn)
{
    return !!(g_cmac_arch_iser_shadow & (1 << irqn));
}

void
os_arch_cmac_disable_irq(IRQn_Type irqn)
{
    __disable_irq();

    NVIC->ICER[0] = 1 << irqn;
    g_cmac_arch_iser_shadow &= ~(1 << irqn);

    __enable_irq();
}

uint32_t
os_arch_cmac_pending_irq(void)
{
    return NVIC->ISPR[0] & g_cmac_arch_iser_shadow;
}

void
os_arch_cmac_wfi(void)
{
    /*
     * It is quite likely that we wake up from "wfi" due to one of bs_ctrl
     * interrupts which will be handled immediately after we enable interrupts.
     * In such case we do not need to wake up idle process so if there are no
     * more pending interrupts (those that are blocked by critical section) we
     * can go to sleep again.
     *
     * We also need to check if F_PENDSV is not set since actual PENDSV is only
     * set when exiting from critical section, so executing "wfi" with F_PENDSV
     * will not cause instant wake up.
     */

    __disable_irq();

    while (!(g_cmac_arch_flags & CMAC_ARCH_F_PENDSV) &&
           !(NVIC->ISPR[0] & g_cmac_arch_iser_shadow & CMAC_ARCH_I_OTHER)) {
        NVIC->ISER[0] = g_cmac_arch_iser_shadow & CMAC_ARCH_I_OTHER;
        __DSB();
        __WFI();
        NVIC->ICER[0] = CMAC_ARCH_I_OTHER;

        __enable_irq();
        __disable_irq();
    }

    __enable_irq();
}

int
os_arch_cmac_deep_sleep(void)
{
    int ret = 0;

    __disable_irq();

    /*
     * Do not execute wfi if PENDSV flag is set since it will be executed only
     * when exiting from critical section and won't prevent wfi from sleeping.
     */
    if (!(g_cmac_arch_flags & CMAC_ARCH_F_PENDSV)) {
        NVIC->ISER[0] = g_cmac_arch_iser_shadow & CMAC_ARCH_I_OTHER;

        ret = cmac_sleep_do_sleep();

        NVIC->ICER[0] = CMAC_ARCH_I_OTHER;
    }

    __enable_irq();

    return ret;
}

void
os_arch_cmac_pendsvset(void)
{
    g_cmac_arch_flags |= CMAC_ARCH_F_PENDSV;
}

/*
 * os_arch_cmac_bs_ctrl_irq_block/unblock shall only be used for controlling
 * transition between BS_CTRL and SW_MAC interrupts. In other cases simply
 * disable interrupt using NVIC_DisableIRQ
 */

void
os_arch_cmac_bs_ctrl_irq_block(void)
{
    assert(!(g_cmac_arch_flags & CMAC_ARCH_F_BS_CTRL_BLOCKED));

    __disable_irq();

    NVIC->ICER[0] = CMAC_ARCH_I_BS_CTRL;
    g_cmac_arch_flags |= CMAC_ARCH_F_BS_CTRL_BLOCKED;

    __enable_irq();
}

void
os_arch_cmac_bs_ctrl_irq_unblock(void)
{
    __disable_irq();

    g_cmac_arch_flags &= ~CMAC_ARCH_F_BS_CTRL_BLOCKED;
    NVIC->ISER[0] = g_cmac_arch_iser_shadow & CMAC_ARCH_I_BS_CTRL;

    __enable_irq();
}

/*
 * os_arch_cmac_idle_section_enter/exit shall only be used when entering and
 * exiting idle handler.
 */

void
os_arch_cmac_idle_section_enter(void)
{
    assert(!(g_cmac_arch_flags & CMAC_ARCH_F_IDLE_SECTION));

    __disable_irq();

    g_cmac_arch_flags |= CMAC_ARCH_F_IDLE_SECTION;
    NVIC->ISER[0] = g_cmac_arch_iser_shadow & CMAC_ARCH_I_HI_PRIO;

    __enable_irq();
}

void
os_arch_cmac_idle_section_exit(void)
{
    assert(g_cmac_arch_flags & CMAC_ARCH_F_IDLE_SECTION);

    __disable_irq();

    NVIC->ICER[0] = CMAC_ARCH_I_HI_PRIO;
    g_cmac_arch_flags &= ~CMAC_ARCH_F_IDLE_SECTION;

    __enable_irq();
}
