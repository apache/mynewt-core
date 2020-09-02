/*
 * Copyright (C) 2019 Dialog Semiconductor. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * - Neither the name of Dialog Semiconductor nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYSTEM_D2763_INCLUDED
#define _SYSTEM_D2763_INCLUDED

// From datasheet.h:

/*--GPIO PID functions-------------------------------------------------------------------------*/
#define FUNC_GPIO              (0)
#define FUNC_UART_RX           (1)
#define FUNC_UART_TX           (2)
#define FUNC_UART2_RX          (3)
#define FUNC_UART2_TX          (4)
#define FUNC_UART2_CTSN        (5)
#define FUNC_UART2_RTSN        (6)
#define FUNC_UART3_RX          (7)
#define FUNC_UART3_TX          (8)
#define FUNC_UART3_CTSN        (9)
#define FUNC_UART3_RTSN        (10)
#define FUNC_ISO_CLK           (11)
#define FUNC_ISO_DATA          (12)
#define FUNC_SPI_DI            (13)
#define FUNC_SPI_DO            (14)
#define FUNC_SPI_CLK           (15)
#define FUNC_SPI_EN            (16)
#define FUNC_SPI2_DI           (17)
#define FUNC_SPI2_DO           (18)
#define FUNC_SPI2_CLK          (19)
#define FUNC_SPI2_EN           (20)
#define FUNC_I2C_SCL           (21)
#define FUNC_I2C_SDA           (22)
#define FUNC_I2C2_SCL          (23)
#define FUNC_I2C2_SDA          (24)
#define FUNC_USB_SOF           (25)
#define FUNC_ADC               (26)
#define FUNC_USB               (27)
#define FUNC_CMAC_DIAG4        (28)
#define FUNC_CMAC_DIAG5        (29)
#define FUNC_CMAC_DIAG6        (30)
#define FUNC_CMAC_DIAG7        (31)
#define FUNC_CMAC_DIAG8        (32)
#define FUNC_CMAC_DIAG9        (33)
#define FUNC_COEX_EXT_ACT      (34)
#define FUNC_COEX_SMART_ACT    (35)
#define FUNC_COEX_SMART_PRI    (36)
#define FUNC_PORT0_DCF         (37)
#define FUNC_PORT1_DCF         (38)
#define FUNC_PORT2_DCF         (39)
#define FUNC_PORT3_DCF         (40)
#define FUNC_PORT4_DCF         (41)
#define FUNC_CLOCK             (42)
#define FUNC_CMAC_DIAG10       (43)
#define FUNC_CMAC_DIAG11       (44)
#define FUNC_CMAC_DIAG12       (45)
#define FUNC_CMAC_DIAG13       (46)
#define FUNC_CMAC_DIAG14       (47)
#define FUNC_CMAC_DIAG15       (48)
#define FUNC_TIM_PWM           (49)
#define FUNC_TIM2_PWM          (50)
#define FUNC_TIM_1SHOT         (51)
#define FUNC_TIM2_1SHOT        (52)
#define FUNC_TIM3_PWM          (53)
#define FUNC_TIM4_PWM          (54)
#define FUNC_AGC_EXT           (55)
#define FUNC_CMAC_DIAG0        (56)
#define FUNC_CMAC_DIAG1        (57)
#define FUNC_CMAC_DIAG2        (58)
#define FUNC_CMAC_DIAG3        (59)
#define FUNC_SWI               (60)
/*--GPIO Direction settings---------------------------------------------------------------------*/
#define DIR_INPUT              0x000
#define DIR_PULLUP             0x100
#define DIR_PULLDOWN           0x200
#define DIR_OUTPUT             0x300

// code copied from global_functions.h

#if defined(CORTEX_M33)
typedef enum {
/* =======================================  ARM Cortex-M33 Specific Interrupt Numbers  ======================================= */
  Reset_IRQn                = -15,              /*!< -15  Reset Vector, invoked on Power up and warm reset                     */
  NonMaskableInt_IRQn       = -14,              /*!< -14  Non maskable Interrupt, cannot be stopped or preempted               */
  HardFault_IRQn            = -13,              /*!< -13  Hard Fault, all classes of Fault                                     */
  MemoryManagement_IRQn     = -12,              /*!< -12  Memory Management, MPU mismatch, including Access Violation
                                                     and No Match                                                              */
  BusFault_IRQn             = -11,              /*!< -11  Bus Fault, Pre-Fetch-, Memory Access Fault, other address/memory
                                                     related Fault                                                             */
  UsageFault_IRQn           = -10,              /*!< -10  Usage Fault, i.e. Undef Instruction, Illegal State Transition        */
  SecureFault_IRQn          =  -9,              /*!< -9 Secure Fault Handler                                                   */
  SVCall_IRQn               =  -5,              /*!< -5 System Service Call via SVC instruction                                */
  DebugMonitor_IRQn         =  -4,              /*!< -4 Debug Monitor                                                          */
  PendSV_IRQn               =  -2,              /*!< -2 Pendable request for system service                                    */
  SysTick_IRQn              =  -1,              /*!< -1 System Tick Timer                                                      */
/* ==========================================  DA1469x Specific Interrupt Numbers  =========================================== */
  SNC_IRQn                  =   0,              /*!< 0  Sensor Node Controller interrupt request.                              */
  DMA_IRQn                  =   1,              /*!< 1  General Purpose DMA interrupt request.                                 */
  CHARGER_STATE_IRQn        =   2,              /*!< 2  Charger State interrupt request.                                       */
  CHARGER_ERROR_IRQn        =   3,              /*!< 3  Charger Error interrupt request.                                       */
  CMAC2SYS_IRQn             =   4,              /*!< 4  CMAC and mailbox interrupt request.                                    */
  UART_IRQn                 =   5,              /*!< 5  UART interrupt request.                                                */
  UART2_IRQn                =   6,              /*!< 6  UART2 interrupt request.                                               */
  UART3_IRQn                =   7,              /*!< 7  UART3 interrupt request.                                               */
  I2C_IRQn                  =   8,              /*!< 8  I2C interrupt request.                                                 */
  I2C2_IRQn                 =   9,              /*!< 9  I2C2 interrupt request.                                                */
  SPI_IRQn                  =  10,              /*!< 10 SPI interrupt request.                                                 */
  SPI2_IRQn                 =  11,              /*!< 11 SPI2 interrupt request.                                                */
  RESERVED12_IRQn           =  12,              /*!< 12 SoftWare interrupt request.                                            */
  RESERVED13_IRQn           =  13,              /*!< 13 SoftWare interrupt request.                                            */
  RESERVED14_IRQn           =  14,              /*!< 14 SoftWare interrupt request.                                            */
  USB_IRQn                  =  15,              /*!< 15 USB interrupt request.                                                 */
  TIMER_IRQn                =  16,              /*!< 16 TIMER interrupt request.                                               */
  TIMER2_IRQn               =  17,              /*!< 17 TIMER2 interrupt request.                                              */
  RTC_IRQn                  =  18,              /*!< 18 RTC interrupt request.                                                 */
  KEY_WKUP_GPIO_IRQn        =  19,              /*!< 19 Debounced button press interrupt request.                              */
  PDC_IRQn                  =  20,              /*!< 20 Wakeup IRQ from PDC to CM33                                            */
  VBUS_IRQn                 =  21,              /*!< 21 VBUS presence interrupt request.                                       */
  MRM_IRQn                  =  22,              /*!< 22 Cache Miss Rate Monitor interrupt request.                             */
  DCDC_BOOST_IRQn           =  23,              /*!< 23 DCDC Boost interrupt request.                                          */
  TRNG_IRQn                 =  24,              /*!< 24 True Random Number Generation interrupt request.                       */
  DCDC_IRQn                 =  25,              /*!< 25 DCDC interrupt request.                                                */
  XTAL32M_RDY_IRQn          =  26,              /*!< 26 XTAL32M trimmed and ready interrupt request.                           */
  GPADC_IRQn                =  27,              /*!< 27 General Purpose Analog-Digital Converter interrupt request.            */
  SDADC_IRQn                =  28,              /*!< 28 Sigma Delta Analog-Digital Converter interrupt request.                */
  CRYPTO_IRQn               =  29,              /*!< 29 Crypto interrupt request.                                              */
  CAPTIMER_IRQn             =  30,              /*!< 30 GPIO triggered Timer Capture interrupt request.                        */
  RFDIAG_IRQn               =  31,              /*!< 31 Baseband or Radio Diagnostics interrupt request.                       */
  RESERVED32_IRQn           =  32,              /*!< 32 SoftWare interrupt request.                                            */
  PLL_LOCK_IRQn             =  33,              /*!< 33 Pll lock interrupt request.                                            */
  TIMER3_IRQn               =  34,              /*!< 34 TIMER3 interrupt request.                                              */
  TIMER4_IRQn               =  35,              /*!< 35 TIMER4 interrupt request.                                              */
  LRA_IRQn                  =  36,              /*!< 36 LRA/ERM interrupt request.                                             */
  RTC_EVENT_IRQn            =  37,              /*!< 37 RTC event interrupt request.                                           */
  GPIO_P0_IRQn              =  38,              /*!< 38 GPIO port 0 toggle interrupt request.                                  */
  GPIO_P1_IRQn              =  39,              /*!< 39 GPIO port 1 toggle interrupt request.                                  */
  SWIC_IRQn                 =  40,              /*!< 40 Single Wire Interface Controller interrupt request.                    */
  RESERVED41_IRQn           =  41,              /*!< 41 SoftWare interrupt request.                                            */
  RESERVED42_IRQn           =  42,              /*!< 42 SoftWare interrupt request.                                            */
  RESERVED43_IRQn           =  43,              /*!< 43 SoftWare interrupt request.                                            */
  RESERVED44_IRQn           =  44,              /*!< 44 SoftWare interrupt request.                                            */
  RESERVED45_IRQn           =  45,              /*!< 45 SoftWare interrupt request.                                            */
  RESERVED46_IRQn           =  46,              /*!< 46 SoftWare interrupt request.                                            */
  RESERVED47_IRQn           =  47               /*!< 47 SoftWare interrupt request.                                            */
} IRQn_Type;


#define __CM33_REV                 0x0000U      /*!< CM33 Core Revision                                                        */
#define __NVIC_PRIO_BITS               3        /*!< Number of Bits used for Priority Levels                                   */
#define __Vendor_SysTickConfig         0        /*!< Set to 1 if different SysTick Config is used                              */
#define __VTOR_PRESENT                 0        /*!< Set to 1 if CPU supports Vector Table Offset Register                     */
#define __MPU_PRESENT                  0        /*!< MPU present or not                                                        */
#define __FPU_PRESENT                  1        /*!< FPU present or not                                                        */
#define __FPU_DP                       0        /*!< Double Precision FPU                                                      */
#define __SAU_REGION_PRESENT           1        /*!< SAU present or not                                                        */
#define __DSP_PRESENT                  1


#include "core_cm33.h"
#ifndef TARGET_LAB
#include "cmsis_mtb.h"
#endif

#else  // if defined(CORTEX_M0PLUS)

typedef enum IRQn {
/****** Cortex-M0 Processor Exceptions Numbers *****************************************/
NMI_IRQn             = -14, /*  2 Non Maskable Interrupt.                              */
HardFault_IRQn       = -13, /*  3 Cortex-M0 Hard Fault Interrupt.                      */
SVCall_IRQn          =  -5, /* 11 Cortex-M0 SV Call Interrupt.                         */
PendSV_IRQn          =  -2, /* 14 Cortex-M0 Pend SV Interrupt.                         */
SysTick_IRQn         =  -1, /* 15 Cortex-M0 System Tick Interrupt.                     */
/****** CMAC CM0P Specific Interrupt Numbers *******************************************/
FIELD_IRQn           =  0,
CALLBACK_IRQn        =  1,
FRAME_IRQn           =  2,
DIAG_IRQn            =  3,
HW_GEN_IRQn          =  4,
SW_MAC_IRQn          =  5,
LL_TIMER2PRMTV_IRQn  =  6,
LL_TIMER2LLC_IRQn    =  7,
CRYPTO_IRQn          =  8,
SW_LLC_1_IRQn        =  9,
SW_LLC_2_IRQn        = 10,
SW_LLC_3_IRQn        = 11,
SYS2CMAC_IRQn        = 12
} IRQn_Type;

/* Configuration of the Cortex-M0+ Processor and Core Peripherals */
#define __CM0_REV                 0x0000    /*!< Core Revision r2p1                               */
#define __NVIC_PRIO_BITS          2         /*!< Number of Bits used for Priority Levels          */
#define __Vendor_SysTickConfig    0         /*!< Set to 1 if different SysTick Config is used     */
//#define __MPU_PRESENT             1       /*!< MPU present or not                               */
//#define __VTOR_PRESENT            1       /*!< Cortex-M0+ can support the VTOR                  */

#include "core_cm0plus.h"                   /* Cortex-M0+ processor and core peripherals          */
//#include "system_CMSDK_CM0plus.h"         /* CMSDK_CM0plus System  include file                 */


#endif // if defined(CORTEX_M0PLUS)

// non-core specific code:

#ifndef __IM                                    /*!< Fallback for older CMSIS versions                                         */
  #define __IM   __I
#endif
#ifndef __OM                                    /*!< Fallback for older CMSIS versions                                         */
  #define __OM   __O
#endif
#ifndef __IOM                                   /*!< Fallback for older CMSIS versions                                         */
  #define __IOM  __IO
#endif


/* ========================================  Start of section using anonymous unions  ======================================== */
#if defined (__CC_ARM)
  #pragma push
  #pragma anon_unions
#elif defined (__ICCARM__)
  #pragma language=extended
#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wc11-extensions"
  #pragma clang diagnostic ignored "-Wreserved-id-macro"
  #pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
  #pragma clang diagnostic ignored "-Wnested-anon-types"
#elif defined (__GNUC__)
  /* anonymous unions are enabled by default */
#elif defined (__TMS470__)
  /* anonymous unions are enabled by default */
#elif defined (__TASKING__)
  #pragma warning 586
#elif defined (__CSMC__)
  /* anonymous unions are enabled by default */
#else
  #warning Not supported compiler type
#endif






#endif //_SYSTEM_D2763_INCLUDED
