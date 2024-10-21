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
 * ports. Port A corresponds to Port 0, B to 1, etc. The nrf54h20 has two ports
 * but Port 1 only has 16 pins.
 */
#define MCU_GPIO_PORTA(pin) ((0 * 16) + (pin))
#define MCU_GPIO_PORTB(pin) ((1 * 16) + (pin))

#define MCU_SYSVIEW_INTERRUPTS \
    "I#1=Reset,I#2=NonMaskableInt,I#3=HardFault,I#4=MemoryManagement,"  \
    "I#5=BusFault,I#6=UsageFault,I#7=SecureFault,I#11=SVCall,"          \
    "I#12=DebugMonitor,I#14=PendSV,I#15=SysTick,I#21=CLOCK_POWER,"      \
    "I#24=RADIO,I#25=RNG,I#26=GPIOTE,I#27=WDT,I#28=TIMER0,I#29=ECB,"    \
    "I#30=AAR_CCM,I#32=TEMP,I#33=RTC0,I#34=IPC,"                        \
    "I#35=SPIM0_SPIS0_TWIM0_TWIS0_UARTE0,I#36=EGU0,I#38=RTC1,I"         \
    "#40=TIMER1,I#41=TIMER2,I#42=SWI0,I#43=SWI1,I#44=SWI2,I#45=SWI3"

#ifdef __cplusplus
}
#endif

#endif /* _MCU_H_ */
