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

#ifndef _MCU_H_
#define _MCU_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Defines for naming GPIOs. NOTE: the nordic chip docs use numeric labels for
 * ports. Port A corresponds to Port 0, B to 1, etc. The nrf5340 has two ports
 * but Port 1 only has 16 pins.
 */
#define MCU_GPIO_PORTA(pin) ((0 * 16) + (pin))
#define MCU_GPIO_PORTB(pin) ((1 * 16) + (pin))

#define MCU_SYSVIEW_INTERRUPTS                                                   \
        "I#1=Reset,I#2=NonMaskableInt,I#3=HardFault,I#4=MemoryManagement,"       \
        "I#5=BusFault,I#6=UsageFault,I#7=SecureFault,I#11=SVCall,"               \
        "I#12=DebugMonitor,I#14=PendSV,I#15=SysTick,I#16=FPU,I#17=CACHE,"        \
        "I#19=SPU,I#21=CLOCK_POWER,I#24=SPIM0_SPIS0_TWIM0_TWIS0_UARTE0,"         \
        "I#25=SPIM1_SPIS1_TWIM1_TWIS1_UARTE1,I#26=SPIM4,"                        \
        "I#27=SPIM2_SPIS2_TWIM2_TWIS2_UARTE2,"                                   \
        "I#28=SPIM3_SPIS3_TWIM3_TWIS3_UARTE3,I#29=GPIOTE0,I#30=SAADC,"           \
        "I#31=TIMER0,I#32=TIMER1,I#33=TIMER2,I#36=RTC0,I#37=RTC1,I#40=WDT0,"     \
        "I#41=WDT1,I#42=COMP_LPCOMP,I#43=EGU0,I#44=EGU1,I#45=EGU2,I#46=EGU3,"    \
        "I#47=EGU4,I#48=EGU5,I#49=PWM0,I#50=PWM1,I#51=PWM2,I#52=PWM3,I#54=PDM0," \
        "I#56=I2S0,I#58=IPC,I#59=QSPI,I#61=NFCT,I#63=GPIOTE1,I#67=QDEC0,"        \
        "I#68=QDEC1,I#70=USBD,I#71=USBREGULATOR,I#73=KMU,I#84=CRYPTOCELL"

#ifdef __cplusplus
}
#endif

#endif /* _MCU_H_ */
