/*
 * Copyright (c) 2019 - 2022, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NRFX_CONFIG_NRF5340_APPLICATION_H__
#define NRFX_CONFIG_NRF5340_APPLICATION_H__

#ifndef NRFX_CONFIG_H__
#error "This file should not be included directly. Include nrfx_config.h instead."
#endif

/*
 * The MDK provides macros for accessing the peripheral register structures
 * by using their secure and non-secure address mappings (with the names
 * containing the suffix _S or _NS, respectively). Because the nrfx drivers
 * use the macros without any suffixes, you must translate the names.
 * The following section provides configuration for the name translation.
 * It must be modified to reflect the actual configuration set in NRF_SPU.
 */
#define NRF_CLOCK        NRF_CLOCK_S
#define NRF_COMP         NRF_COMP_S
#define NRF_DCNF         NRF_DCNF_S
#define NRF_DPPIC        NRF_DPPIC_S
#define NRF_EGU0         NRF_EGU0_S
#define NRF_EGU1         NRF_EGU1_S
#define NRF_EGU2         NRF_EGU2_S
#define NRF_EGU3         NRF_EGU3_S
#define NRF_EGU4         NRF_EGU4_S
#define NRF_EGU5         NRF_EGU5_S
#define NRF_FPU          NRF_FPU_S
#define NRF_I2S0         NRF_I2S0_S
#define NRF_IPC          NRF_IPC_S
#define NRF_KMU          NRF_KMU_S
#define NRF_LPCOMP       NRF_LPCOMP_S
#define NRF_MUTEX        NRF_MUTEX_S
#define NRF_NFCT         NRF_NFCT_S
#define NRF_NVMC         NRF_NVMC_S
#define NRF_OSCILLATORS  NRF_OSCILLATORS_S
#define NRF_P0           NRF_P0_S
#define NRF_P1           NRF_P1_S
#define NRF_PDM0         NRF_PDM0_S
#define NRF_POWER        NRF_POWER_S
#define NRF_PWM0         NRF_PWM0_S
#define NRF_PWM1         NRF_PWM1_S
#define NRF_PWM2         NRF_PWM2_S
#define NRF_PWM3         NRF_PWM3_S
#define NRF_QDEC0        NRF_QDEC0_S
#define NRF_QDEC1        NRF_QDEC1_S
#define NRF_QSPI         NRF_QSPI_S
#define NRF_REGULATORS   NRF_REGULATORS_S
#define NRF_RESET        NRF_RESET_S
#define NRF_RTC0         NRF_RTC0_S
#define NRF_RTC1         NRF_RTC1_S
#define NRF_SAADC        NRF_SAADC_S
#define NRF_SPIM0        NRF_SPIM0_S
#define NRF_SPIM1        NRF_SPIM1_S
#define NRF_SPIM2        NRF_SPIM2_S
#define NRF_SPIM3        NRF_SPIM3_S
#define NRF_SPIM4        NRF_SPIM4_S
#define NRF_SPIS0        NRF_SPIS0_S
#define NRF_SPIS1        NRF_SPIS1_S
#define NRF_SPIS2        NRF_SPIS2_S
#define NRF_SPIS3        NRF_SPIS3_S
#define NRF_TIMER0       NRF_TIMER0_S
#define NRF_TIMER1       NRF_TIMER1_S
#define NRF_TIMER2       NRF_TIMER2_S
#define NRF_TWIM0        NRF_TWIM0_S
#define NRF_TWIM1        NRF_TWIM1_S
#define NRF_TWIM2        NRF_TWIM2_S
#define NRF_TWIM3        NRF_TWIM3_S
#define NRF_TWIS0        NRF_TWIS0_S
#define NRF_TWIS1        NRF_TWIS1_S
#define NRF_TWIS2        NRF_TWIS2_S
#define NRF_TWIS3        NRF_TWIS3_S
#define NRF_UARTE0       NRF_UARTE0_S
#define NRF_UARTE1       NRF_UARTE1_S
#define NRF_UARTE2       NRF_UARTE2_S
#define NRF_UARTE3       NRF_UARTE3_S
#define NRF_USBD         NRF_USBD_S
#define NRF_USBREGULATOR NRF_USBREGULATOR_S
#define NRF_VMC          NRF_VMC_S
#define NRF_WDT0         NRF_WDT0_S
#define NRF_WDT1         NRF_WDT1_S

/*
 * The following section provides the name translation for peripherals with
 * only one type of access available. For these peripherals, you cannot choose
 * between secure and non-secure mapping.
 */
#if defined(NRF_TRUSTZONE_NONSECURE)
#define NRF_GPIOTE1      NRF_GPIOTE1_NS
#else
#define NRF_CACHE        NRF_CACHE_S
#define NRF_CACHEINFO    NRF_CACHEINFO_S
#define NRF_CACHEDATA    NRF_CACHEDATA_S
#define NRF_CRYPTOCELL   NRF_CRYPTOCELL_S
#define NRF_CTI          NRF_CTI_S
#define NRF_FICR         NRF_FICR_S
#define NRF_GPIOTE0      NRF_GPIOTE0_S
#define NRF_SPU          NRF_SPU_S
#define NRF_TAD          NRF_TAD_S
#define NRF_UICR         NRF_UICR_S
#endif

/* Fixups for the GPIOTE driver. */
#if defined(NRF_TRUSTZONE_NONSECURE)
#define NRF_GPIOTE        NRF_GPIOTE1
#define GPIOTE_IRQHandler GPIOTE1_IRQHandler
#else
#define NRF_GPIOTE        NRF_GPIOTE0
#define GPIOTE_IRQHandler GPIOTE0_IRQHandler
#endif

/* Fixups for the QDEC driver. */
#define NRF_QDEC        NRF_QDEC0
#define QDEC_IRQHandler QDEC0_IRQHandler

// <<< Use Configuration Wizard in Context Menu >>>\n

// <h> nRF_Drivers

// <e> NRFX_CLOCK_ENABLED - nrfx_clock - CLOCK peripheral driver.
//==========================================================
#ifndef NRFX_CLOCK_ENABLED
#define NRFX_CLOCK_ENABLED 0
#endif
// <o> NRFX_CLOCK_CONFIG_LF_SRC  - LF clock source.

// <0=> ULP
// <1=> RC
// <2=> XTAL
// <3=> Synth

#ifndef NRFX_CLOCK_CONFIG_LF_SRC
#define NRFX_CLOCK_CONFIG_LF_SRC 2
#endif

// <q> NRFX_CLOCK_CONFIG_LF_CAL_ENABLED  - Enables LF Clock Calibration Support

#ifndef NRFX_CLOCK_CONFIG_LF_CAL_ENABLED
#define NRFX_CLOCK_CONFIG_LF_CAL_ENABLED 0
#endif

// <q> NRFX_CLOCK_CONFIG_LFXO_TWO_STAGE_ENABLED - Enables two-stage LFXO start procedure

// <i> If set to a non-zero value, LFRC will be started before LFXO and corresponding
// <i> event will be generated. It means that CPU will be woken up when LFRC
// <i> oscillator starts, but user callback will be invoked only after LFXO
// <i> finally starts.

#ifndef NRFX_CLOCK_CONFIG_LFXO_TWO_STAGE_ENABLED
#define NRFX_CLOCK_CONFIG_LFXO_TWO_STAGE_ENABLED 0
#endif

// <o> NRFX_CLOCK_CONFIG_HFCLK192M_SRC  - HFCLK192M source.

// <0=> HFINT
// <1=> HFXO

#ifndef NRFX_CLOCK_CONFIG_HFCLK192M_SRC
#define NRFX_CLOCK_CONFIG_HFCLK192M_SRC 1
#endif

// <o> NRFX_CLOCK_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_CLOCK_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_CLOCK_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_CLOCK_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_CLOCK_CONFIG_LOG_ENABLED
#define NRFX_CLOCK_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_CLOCK_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_CLOCK_CONFIG_LOG_LEVEL
#define NRFX_CLOCK_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_CLOCK_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_CLOCK_CONFIG_INFO_COLOR
#define NRFX_CLOCK_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_CLOCK_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_CLOCK_CONFIG_DEBUG_COLOR
#define NRFX_CLOCK_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_COMP_ENABLED - nrfx_comp - COMP peripheral driver
//==========================================================
#ifndef NRFX_COMP_ENABLED
#define NRFX_COMP_ENABLED 0
#endif

// <o> NRFX_COMP_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_COMP_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_COMP_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_COMP_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_COMP_CONFIG_LOG_ENABLED
#define NRFX_COMP_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_COMP_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_COMP_CONFIG_LOG_LEVEL
#define NRFX_COMP_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_COMP_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_COMP_CONFIG_INFO_COLOR
#define NRFX_COMP_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_COMP_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_COMP_CONFIG_DEBUG_COLOR
#define NRFX_COMP_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_DPPI_ENABLED - nrfx_dppi - DPPI allocator.
//==========================================================
#ifndef NRFX_DPPI_ENABLED
#define NRFX_DPPI_ENABLED 0
#endif
// <e> NRFX_DPPI_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_DPPI_CONFIG_LOG_ENABLED
#define NRFX_DPPI_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_DPPI_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_DPPI_CONFIG_LOG_LEVEL
#define NRFX_DPPI_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_DPPI_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_DPPI_CONFIG_INFO_COLOR
#define NRFX_DPPI_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_DPPI_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_DPPI_CONFIG_DEBUG_COLOR
#define NRFX_DPPI_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_EGU_ENABLED - nrfx_egu - EGU peripheral driver.
//==========================================================
#ifndef NRFX_EGU_ENABLED
#define NRFX_EGU_ENABLED 0
#endif

// <q> NRFX_EGU0_ENABLED  - Enable EGU0 instance.

#ifndef NRFX_EGU0_ENABLED
#define NRFX_EGU0_ENABLED 0
#endif

// <q> NRFX_EGU1_ENABLED  - Enable EGU1 instance.

#ifndef NRFX_EGU1_ENABLED
#define NRFX_EGU1_ENABLED 0
#endif

// <q> NRFX_EGU2_ENABLED  - Enable EGU2 instance.

#ifndef NRFX_EGU2_ENABLED
#define NRFX_EGU2_ENABLED 0
#endif

// <q> NRFX_EGU3_ENABLED  - Enable EGU3 instance.

#ifndef NRFX_EGU3_ENABLED
#define NRFX_EGU3_ENABLED 0
#endif

// <q> NRFX_EGU4_ENABLED  - Enable EGU4 instance.

#ifndef NRFX_EGU4_ENABLED
#define NRFX_EGU4_ENABLED 0
#endif

// <q> NRFX_EGU5_ENABLED  - Enable EGU5 instance.

#ifndef NRFX_EGU5_ENABLED
#define NRFX_EGU5_ENABLED 0
#endif

// <o> NRFX_EGU_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_EGU_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_EGU_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// </e>

// <e> NRFX_GPIOTE_ENABLED - nrfx_gpiote - GPIOTE peripheral driver.
//==========================================================
#ifndef NRFX_GPIOTE_ENABLED
#define NRFX_GPIOTE_ENABLED 0
#endif

// <o> NRFX_GPIOTE_CONFIG_NUM_OF_EVT_HANDLERS - Number of dedicated handlers
#ifndef NRFX_GPIOTE_CONFIG_NUM_OF_EVT_HANDLERS
#define NRFX_GPIOTE_CONFIG_NUM_OF_EVT_HANDLERS 2
#endif

// <o> NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_GPIOTE_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_GPIOTE_CONFIG_LOG_ENABLED
#define NRFX_GPIOTE_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_GPIOTE_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_GPIOTE_CONFIG_LOG_LEVEL
#define NRFX_GPIOTE_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_GPIOTE_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_GPIOTE_CONFIG_INFO_COLOR
#define NRFX_GPIOTE_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_GPIOTE_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_GPIOTE_CONFIG_DEBUG_COLOR
#define NRFX_GPIOTE_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_I2S_ENABLED - nrfx_i2s - I2S peripheral driver.
//==========================================================
#ifndef NRFX_I2S_ENABLED
#define NRFX_I2S_ENABLED 0
#endif

// <o> NRFX_I2S_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_I2S_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_I2S_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_I2S_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_I2S_CONFIG_LOG_ENABLED
#define NRFX_I2S_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_I2S_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_I2S_CONFIG_LOG_LEVEL
#define NRFX_I2S_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_I2S_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_I2S_CONFIG_INFO_COLOR
#define NRFX_I2S_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_I2S_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_I2S_CONFIG_DEBUG_COLOR
#define NRFX_I2S_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_IPC_ENABLED - nrfx_ipc - IPC peripheral driver
//==========================================================
#ifndef NRFX_IPC_ENABLED
#define NRFX_IPC_ENABLED 0
#endif

// </e>

// <e> NRFX_LPCOMP_ENABLED - nrfx_lpcomp - LPCOMP peripheral driver
//==========================================================
#ifndef NRFX_LPCOMP_ENABLED
#define NRFX_LPCOMP_ENABLED 0
#endif

// <o> NRFX_LPCOMP_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_LPCOMP_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_LPCOMP_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_LPCOMP_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_LPCOMP_CONFIG_LOG_ENABLED
#define NRFX_LPCOMP_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_LPCOMP_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_LPCOMP_CONFIG_LOG_LEVEL
#define NRFX_LPCOMP_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_LPCOMP_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_LPCOMP_CONFIG_INFO_COLOR
#define NRFX_LPCOMP_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_LPCOMP_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_LPCOMP_CONFIG_DEBUG_COLOR
#define NRFX_LPCOMP_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_NFCT_ENABLED - nrfx_nfct - NFCT peripheral driver
//==========================================================
#ifndef NRFX_NFCT_ENABLED
#define NRFX_NFCT_ENABLED 0
#endif
// <o> NRFX_NFCT_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_NFCT_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_NFCT_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <o> NRFX_NFCT_CONFIG_TIMER_INSTANCE_ID - Timer instance used for workarounds in the driver.

// <0=> 0
// <1=> 1
// <2=> 2

#ifndef NRFX_NFCT_CONFIG_TIMER_INSTANCE_ID
#define NRFX_NFCT_CONFIG_TIMER_INSTANCE_ID 2
#endif

// <e> NRFX_NFCT_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_NFCT_CONFIG_LOG_ENABLED
#define NRFX_NFCT_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_NFCT_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_NFCT_CONFIG_LOG_LEVEL
#define NRFX_NFCT_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_NFCT_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_NFCT_CONFIG_INFO_COLOR
#define NRFX_NFCT_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_NFCT_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_NFCT_CONFIG_DEBUG_COLOR
#define NRFX_NFCT_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_NVMC_ENABLED - nrfx_nvmc - NVMC peripheral driver
//==========================================================
#ifndef NRFX_NVMC_ENABLED
#define NRFX_NVMC_ENABLED 0
#endif

// </e>

// <e> NRFX_PDM_ENABLED - nrfx_pdm - PDM peripheral driver.
//==========================================================
#ifndef NRFX_PDM_ENABLED
#define NRFX_PDM_ENABLED 0
#endif

// <o> NRFX_PDM_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_PDM_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_PDM_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_PDM_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_PDM_CONFIG_LOG_ENABLED
#define NRFX_PDM_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_PDM_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_PDM_CONFIG_LOG_LEVEL
#define NRFX_PDM_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_PDM_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_PDM_CONFIG_INFO_COLOR
#define NRFX_PDM_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_PDM_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_PDM_CONFIG_DEBUG_COLOR
#define NRFX_PDM_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_POWER_ENABLED - nrfx_power - POWER peripheral driver.
//==========================================================
#ifndef NRFX_POWER_ENABLED
#define NRFX_POWER_ENABLED 0
#endif
// <o> NRFX_POWER_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_POWER_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_POWER_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// </e>

// <e> NRFX_PRS_ENABLED - nrfx_prs - Peripheral Resource Sharing (PRS) module.
//==========================================================
#ifndef NRFX_PRS_ENABLED
#define NRFX_PRS_ENABLED 0
#endif
// <q> NRFX_PRS_BOX_0_ENABLED  - Enables box 0 in the module.


#ifndef NRFX_PRS_BOX_0_ENABLED
#define NRFX_PRS_BOX_0_ENABLED 0
#endif

// <q> NRFX_PRS_BOX_1_ENABLED  - Enables box 1 in the module.


#ifndef NRFX_PRS_BOX_1_ENABLED
#define NRFX_PRS_BOX_1_ENABLED 0
#endif

// <q> NRFX_PRS_BOX_2_ENABLED  - Enables box 2 in the module.


#ifndef NRFX_PRS_BOX_2_ENABLED
#define NRFX_PRS_BOX_2_ENABLED 0
#endif

// <q> NRFX_PRS_BOX_3_ENABLED  - Enables box 3 in the module.


#ifndef NRFX_PRS_BOX_3_ENABLED
#define NRFX_PRS_BOX_3_ENABLED 0
#endif

// <q> NRFX_PRS_BOX_4_ENABLED  - Enables box 4 in the module.


#ifndef NRFX_PRS_BOX_4_ENABLED
#define NRFX_PRS_BOX_4_ENABLED 0
#endif


// <e> NRFX_PRS_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_PRS_CONFIG_LOG_ENABLED
#define NRFX_PRS_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_PRS_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_PRS_CONFIG_LOG_LEVEL
#define NRFX_PRS_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_PRS_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_PRS_CONFIG_INFO_COLOR
#define NRFX_PRS_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_PRS_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_PRS_CONFIG_DEBUG_COLOR
#define NRFX_PRS_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_PWM_ENABLED - nrfx_pwm - PWM peripheral driver.
//==========================================================
#ifndef NRFX_PWM_ENABLED
#define NRFX_PWM_ENABLED 0
#endif
// <q> NRFX_PWM0_ENABLED  - Enables PWM0 instance.


#ifndef NRFX_PWM0_ENABLED
#define NRFX_PWM0_ENABLED 0
#endif

// <q> NRFX_PWM1_ENABLED  - Enables PWM1 instance.


#ifndef NRFX_PWM1_ENABLED
#define NRFX_PWM1_ENABLED 0
#endif

// <q> NRFX_PWM2_ENABLED  - Enables PWM2 instance.


#ifndef NRFX_PWM2_ENABLED
#define NRFX_PWM2_ENABLED 0
#endif

// <q> NRFX_PWM3_ENABLED  - Enables PWM3 instance.


#ifndef NRFX_PWM3_ENABLED
#define NRFX_PWM3_ENABLED 0
#endif

// <o> NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_PWM_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_PWM_CONFIG_LOG_ENABLED
#define NRFX_PWM_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_PWM_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_PWM_CONFIG_LOG_LEVEL
#define NRFX_PWM_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_PWM_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_PWM_CONFIG_INFO_COLOR
#define NRFX_PWM_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_PWM_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_PWM_CONFIG_DEBUG_COLOR
#define NRFX_PWM_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_QDEC_ENABLED - nrfx_qdec - QDEC peripheral driver
//==========================================================
#ifndef NRFX_QDEC_ENABLED
#define NRFX_QDEC_ENABLED 0
#endif

// <o> NRFX_QDEC_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_QDEC_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_QDEC_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_QDEC_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_QDEC_CONFIG_LOG_ENABLED
#define NRFX_QDEC_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_QDEC_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_QDEC_CONFIG_LOG_LEVEL
#define NRFX_QDEC_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_QDEC_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_QDEC_CONFIG_INFO_COLOR
#define NRFX_QDEC_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_QDEC_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_QDEC_CONFIG_DEBUG_COLOR
#define NRFX_QDEC_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_QSPI_ENABLED - nrfx_qspi - QSPI peripheral driver
//==========================================================
#ifndef NRFX_QSPI_ENABLED
#define NRFX_QSPI_ENABLED 0
#endif

// <o> NRFX_QSPI_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_QSPI_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_QSPI_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// </e>

// <e> NRFX_RTC_ENABLED - nrfx_rtc - RTC peripheral driver.
//==========================================================
#ifndef NRFX_RTC_ENABLED
#define NRFX_RTC_ENABLED 0
#endif
// <q> NRFX_RTC0_ENABLED  - Enables RTC0 instance.


#ifndef NRFX_RTC0_ENABLED
#define NRFX_RTC0_ENABLED 0
#endif

// <q> NRFX_RTC1_ENABLED  - Enables RTC1 instance.


#ifndef NRFX_RTC1_ENABLED
#define NRFX_RTC1_ENABLED 0
#endif

// <o> NRFX_RTC_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_RTC_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_RTC_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_RTC_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_RTC_CONFIG_LOG_ENABLED
#define NRFX_RTC_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_RTC_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_RTC_CONFIG_LOG_LEVEL
#define NRFX_RTC_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_RTC_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_RTC_CONFIG_INFO_COLOR
#define NRFX_RTC_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_RTC_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_RTC_CONFIG_DEBUG_COLOR
#define NRFX_RTC_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_SAADC_ENABLED - nrfx_saadc - SAADC peripheral driver.
//==========================================================
#ifndef NRFX_SAADC_ENABLED
#define NRFX_SAADC_ENABLED 0
#endif

// <o> NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_SAADC_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_SAADC_CONFIG_LOG_ENABLED
#define NRFX_SAADC_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_SAADC_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_SAADC_CONFIG_LOG_LEVEL
#define NRFX_SAADC_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_SAADC_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_SAADC_CONFIG_INFO_COLOR
#define NRFX_SAADC_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_SAADC_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_SAADC_CONFIG_DEBUG_COLOR
#define NRFX_SAADC_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_SPIM_ENABLED - nrfx_spim - SPIM peripheral driver.
//==========================================================
#ifndef NRFX_SPIM_ENABLED
#define NRFX_SPIM_ENABLED 0
#endif
// <q> NRFX_SPIM0_ENABLED  - Enables SPIM0 instance.


#ifndef NRFX_SPIM0_ENABLED
#define NRFX_SPIM0_ENABLED 0
#endif

// <q> NRFX_SPIM1_ENABLED  - Enables SPIM1 instance.


#ifndef NRFX_SPIM1_ENABLED
#define NRFX_SPIM1_ENABLED 0
#endif

// <q> NRFX_SPIM2_ENABLED  - Enables SPIM2 instance.


#ifndef NRFX_SPIM2_ENABLED
#define NRFX_SPIM2_ENABLED 0
#endif

// <q> NRFX_SPIM3_ENABLED  - Enables SPIM3 instance.


#ifndef NRFX_SPIM3_ENABLED
#define NRFX_SPIM3_ENABLED 0
#endif

// <q> NRFX_SPIM4_ENABLED  - Enables SPIM4 instance.


#ifndef NRFX_SPIM4_ENABLED
#define NRFX_SPIM4_ENABLED 0
#endif

// <q> NRFX_SPIM_EXTENDED_ENABLED  - Enable extended SPIM features


#ifndef NRFX_SPIM_EXTENDED_ENABLED
#define NRFX_SPIM_EXTENDED_ENABLED 0
#endif

// <o> NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_SPIM_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_SPIM_CONFIG_LOG_ENABLED
#define NRFX_SPIM_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_SPIM_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_SPIM_CONFIG_LOG_LEVEL
#define NRFX_SPIM_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_SPIM_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_SPIM_CONFIG_INFO_COLOR
#define NRFX_SPIM_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_SPIM_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_SPIM_CONFIG_DEBUG_COLOR
#define NRFX_SPIM_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_SPIS_ENABLED - nrfx_spis - SPIS peripheral driver.
//==========================================================
#ifndef NRFX_SPIS_ENABLED
#define NRFX_SPIS_ENABLED 0
#endif
// <q> NRFX_SPIS0_ENABLED  - Enables SPIS0 instance.


#ifndef NRFX_SPIS0_ENABLED
#define NRFX_SPIS0_ENABLED 0
#endif

// <q> NRFX_SPIS1_ENABLED  - Enables SPIS1 instance.


#ifndef NRFX_SPIS1_ENABLED
#define NRFX_SPIS1_ENABLED 0
#endif

// <q> NRFX_SPIS2_ENABLED  - Enables SPIS2 instance.


#ifndef NRFX_SPIS2_ENABLED
#define NRFX_SPIS2_ENABLED 0
#endif

// <q> NRFX_SPIS3_ENABLED  - Enables SPIS3 instance.


#ifndef NRFX_SPIS3_ENABLED
#define NRFX_SPIS3_ENABLED 0
#endif

// <o> NRFX_SPIS_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_SPIS_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_SPIS_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_SPIS_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_SPIS_CONFIG_LOG_ENABLED
#define NRFX_SPIS_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_SPIS_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_SPIS_CONFIG_LOG_LEVEL
#define NRFX_SPIS_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_SPIS_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_SPIS_CONFIG_INFO_COLOR
#define NRFX_SPIS_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_SPIS_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_SPIS_CONFIG_DEBUG_COLOR
#define NRFX_SPIS_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <q> NRFX_SYSTICK_ENABLED  - nrfx_systick - ARM(R) SysTick driver.


#ifndef NRFX_SYSTICK_ENABLED
#define NRFX_SYSTICK_ENABLED 0
#endif

// <e> NRFX_TIMER_ENABLED - nrfx_timer - TIMER periperal driver.
//==========================================================
#ifndef NRFX_TIMER_ENABLED
#define NRFX_TIMER_ENABLED 0
#endif
// <q> NRFX_TIMER0_ENABLED  - Enables TIMER0 instance.


#ifndef NRFX_TIMER0_ENABLED
#define NRFX_TIMER0_ENABLED 0
#endif

// <q> NRFX_TIMER1_ENABLED  - Enables TIMER1 instance.


#ifndef NRFX_TIMER1_ENABLED
#define NRFX_TIMER1_ENABLED 0
#endif

// <q> NRFX_TIMER2_ENABLED  - Enables TIMER2 instance.


#ifndef NRFX_TIMER2_ENABLED
#define NRFX_TIMER2_ENABLED 0
#endif

// <o> NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_TIMER_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_TIMER_CONFIG_LOG_ENABLED
#define NRFX_TIMER_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_TIMER_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_TIMER_CONFIG_LOG_LEVEL
#define NRFX_TIMER_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_TIMER_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_TIMER_CONFIG_INFO_COLOR
#define NRFX_TIMER_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_TIMER_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_TIMER_CONFIG_DEBUG_COLOR
#define NRFX_TIMER_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_TWIM_ENABLED - nrfx_twim - TWIM peripheral driver.
//==========================================================
#ifndef NRFX_TWIM_ENABLED
#define NRFX_TWIM_ENABLED 0
#endif
// <q> NRFX_TWIM0_ENABLED  - Enables TWIM0 instance.


#ifndef NRFX_TWIM0_ENABLED
#define NRFX_TWIM0_ENABLED 0
#endif

// <q> NRFX_TWIM1_ENABLED  - Enables TWIM1 instance.


#ifndef NRFX_TWIM1_ENABLED
#define NRFX_TWIM1_ENABLED 0
#endif

// <q> NRFX_TWIM2_ENABLED  - Enables TWIM2 instance.


#ifndef NRFX_TWIM2_ENABLED
#define NRFX_TWIM2_ENABLED 0
#endif

// <q> NRFX_TWIM3_ENABLED  - Enables TWIM3 instance.


#ifndef NRFX_TWIM3_ENABLED
#define NRFX_TWIM3_ENABLED 0
#endif

// <o> NRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_TWIM_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_TWIM_CONFIG_LOG_ENABLED
#define NRFX_TWIM_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_TWIM_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_TWIM_CONFIG_LOG_LEVEL
#define NRFX_TWIM_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_TWIM_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_TWIM_CONFIG_INFO_COLOR
#define NRFX_TWIM_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_TWIM_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_TWIM_CONFIG_DEBUG_COLOR
#define NRFX_TWIM_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_TWIS_ENABLED - nrfx_twis - TWIS peripheral driver.
//==========================================================
#ifndef NRFX_TWIS_ENABLED
#define NRFX_TWIS_ENABLED 0
#endif

// <q> NRFX_TWIS0_ENABLED  - Enables TWIS0 instance.

#ifndef NRFX_TWIS0_ENABLED
#define NRFX_TWIS0_ENABLED 0
#endif

// <q> NRFX_TWIS1_ENABLED  - Enables TWIS1 instance.

#ifndef NRFX_TWIS1_ENABLED
#define NRFX_TWIS1_ENABLED 0
#endif

// <q> NRFX_TWIS2_ENABLED  - Enables TWIS2 instance.

#ifndef NRFX_TWIS2_ENABLED
#define NRFX_TWIS2_ENABLED 0
#endif

// <q> NRFX_TWIS3_ENABLED  - Enables TWIS3 instance.

#ifndef NRFX_TWIS3_ENABLED
#define NRFX_TWIS3_ENABLED 0
#endif

// <q> NRFX_TWIS_ASSUME_INIT_AFTER_RESET_ONLY  - Assumes that any instance would be initialized only once.

// <i> Optimization flag. Registers used by TWIS are shared by other peripherals. Normally, during initialization driver tries to clear all registers to known state before doing the initialization itself. This gives initialization safe procedure, no matter when it would be called. If you activate TWIS only once and do never uninitialize it - set this flag to 1 what gives more optimal code.

#ifndef NRFX_TWIS_ASSUME_INIT_AFTER_RESET_ONLY
#define NRFX_TWIS_ASSUME_INIT_AFTER_RESET_ONLY 0
#endif

// <q> NRFX_TWIS_NO_SYNC_MODE  - Removes support for synchronous mode.

// <i> Synchronous mode would be used in specific situations. And it uses some additional code and data memory to safely process state machine by polling it in status functions. If this functionality is not required it may be disabled to free some resources.

#ifndef NRFX_TWIS_NO_SYNC_MODE
#define NRFX_TWIS_NO_SYNC_MODE 0
#endif

// <o> NRFX_TWIS_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_TWIS_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_TWIS_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_TWIS_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_TWIS_CONFIG_LOG_ENABLED
#define NRFX_TWIS_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_TWIS_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_TWIS_CONFIG_LOG_LEVEL
#define NRFX_TWIS_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_TWIS_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_TWIS_CONFIG_INFO_COLOR
#define NRFX_TWIS_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_TWIS_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_TWIS_CONFIG_DEBUG_COLOR
#define NRFX_TWIS_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_UARTE_ENABLED - nrfx_uarte - UARTE peripheral driver.
//==========================================================
#ifndef NRFX_UARTE_ENABLED
#define NRFX_UARTE_ENABLED 0
#endif
// <o> NRFX_UARTE0_ENABLED - Enables UARTE0 instances
#ifndef NRFX_UARTE0_ENABLED
#define NRFX_UARTE0_ENABLED 0
#endif

// <o> NRFX_UARTE1_ENABLED - Enables UARTE1 instance.
#ifndef NRFX_UARTE1_ENABLED
#define NRFX_UARTE1_ENABLED 0
#endif

// <o> NRFX_UARTE2_ENABLED - Enables UARTE2 instance.
#ifndef NRFX_UARTE2_ENABLED
#define NRFX_UARTE2_ENABLED 0
#endif

// <o> NRFX_UARTE3_ENABLED - Enables UARTE3 instance.
#ifndef NRFX_UARTE3_ENABLED
#define NRFX_UARTE3_ENABLED 0
#endif

// <o> NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_UARTE_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_UARTE_CONFIG_LOG_ENABLED
#define NRFX_UARTE_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_UARTE_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_UARTE_CONFIG_LOG_LEVEL
#define NRFX_UARTE_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_UARTE_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_UARTE_CONFIG_INFO_COLOR
#define NRFX_UARTE_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_UARTE_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_UARTE_CONFIG_DEBUG_COLOR
#define NRFX_UARTE_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_USBD_ENABLED - nrfx_usbd - USBD peripheral driver
//==========================================================
#ifndef NRFX_USBD_ENABLED
#define NRFX_USBD_ENABLED 0
#endif
// <o> NRFX_USBD_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_USBD_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_USBD_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <q> USBD_CONFIG_DMASCHEDULER_ISO_BOOST  - Give priority to isochronous transfers

// <i> This option gives priority to isochronous transfers.
// <i> Enabling it assures that isochronous transfers are always processed,
// <i> even if multiple other transfers are pending.
// <i> Isochronous endpoints are prioritized before the usbd_dma_scheduler_algorithm
// <i> function is called, so the option is independent of the algorithm chosen.

#ifndef NRFX_USBD_CONFIG_DMASCHEDULER_ISO_BOOST
#define NRFX_USBD_CONFIG_DMASCHEDULER_ISO_BOOST 1
#endif

// <q> USBD_CONFIG_ISO_IN_ZLP  - Respond to an IN token on ISO IN endpoint with ZLP when no data is ready


// <i> If set, ISO IN endpoint will respond to an IN token with ZLP when no data is ready to be sent.
// <i> Else, there will be no response.

#ifndef NRFX_USBD_CONFIG_ISO_IN_ZLP
#define NRFX_USBD_CONFIG_ISO_IN_ZLP 0
#endif

// <e> NRFX_USBD_CONFIG_LOG_ENABLED - Enable logging in the module
//==========================================================
#ifndef NRFX_USBD_CONFIG_LOG_ENABLED
#define NRFX_USBD_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_USBD_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_USBD_CONFIG_LOG_LEVEL
#define NRFX_USBD_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_USBD_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_USBD_CONFIG_INFO_COLOR
#define NRFX_USBD_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_USBD_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_USBD_CONFIG_DEBUG_COLOR
#define NRFX_USBD_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_USBREG_ENABLED - nrfx_usbreg - USBREG peripheral driver
//==========================================================
#ifndef NRFX_USBREG_ENABLED
#define NRFX_USBREG_ENABLED 0
#endif
// <o> NRFX_USBREG_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_USBREG_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_USBREG_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// </e>

// </e>

// <e> NRFX_WDT_ENABLED - nrfx_wdt - WDT peripheral driver.
//==========================================================
#ifndef NRFX_WDT_ENABLED
#define NRFX_WDT_ENABLED 0
#endif
// <q> NRFX_WDT0_ENABLED  - Enable WDT0 instance.


#ifndef NRFX_WDT0_ENABLED
#define NRFX_WDT0_ENABLED 0
#endif

// <q> NRFX_WDT1_ENABLED  - Enable WDT1 instance.


#ifndef NRFX_WDT1_ENABLED
#define NRFX_WDT1_ENABLED 0
#endif

// <o> NRFX_WDT_CONFIG_NO_IRQ  - Remove WDT IRQ handling from WDT driver.

// <0=> Include WDT IRQ handling
// <1=> Remove WDT IRQ handling

#ifndef NRFX_WDT_CONFIG_NO_IRQ
#define NRFX_WDT_CONFIG_NO_IRQ 0
#endif

// <o> NRFX_WDT_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority.

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_WDT_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_WDT_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_WDT_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_WDT_CONFIG_LOG_ENABLED
#define NRFX_WDT_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_WDT_CONFIG_LOG_LEVEL  - Default severity level.

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_WDT_CONFIG_LOG_LEVEL
#define NRFX_WDT_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_WDT_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_WDT_CONFIG_INFO_COLOR
#define NRFX_WDT_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_WDT_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_WDT_CONFIG_DEBUG_COLOR
#define NRFX_WDT_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// </h>

#endif // NRFX_CONFIG_NRF5340_APPLICATION_H__
