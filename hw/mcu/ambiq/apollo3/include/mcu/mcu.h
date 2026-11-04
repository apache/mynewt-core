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

#ifndef __MCU_MCU_H_
#define __MCU_MCU_H_

#include <mcu/apollo3.h>

/*
 * Apollo GPIO pins are a single flat range; no MCU_GPIO_PORTx() macros
 * are defined, so callers fall back to plain pin numbers.
 */

/*
 * SystemView interrupt descriptions. Exception numbers: external IRQn + 16.
 */
#define MCU_SYSVIEW_INTERRUPTS                                                \
    "I#1=Reset,I#2=NonMaskableInt,I#3=HardFault,I#4=MemoryManagement,"        \
    "I#5=BusFault,I#6=UsageFault,I#11=SVCall,I#12=DebugMonitor,"              \
    "I#14=PendSV,I#15=SysTick,I#16=BROWNOUT,I#17=WDT,I#18=RTC,I#19=VCOMP,"    \
    "I#20=IOSLAVE,I#21=IOSLAVEACC,I#22=IOMSTR0,I#23=IOMSTR1,I#24=IOMSTR2,"    \
    "I#25=IOMSTR3,I#26=IOMSTR4,I#27=IOMSTR5,I#28=BLE,I#29=GPIO,I#30=CTIMER,"  \
    "I#31=UART0,I#32=UART1,I#33=SCARD,I#34=ADC,I#35=PDM,I#36=MSPI0,"          \
    "I#38=STIMER,I#39=STIMER_CMPR0,I#40=STIMER_CMPR1,I#41=STIMER_CMPR2,"      \
    "I#42=STIMER_CMPR3,I#43=STIMER_CMPR4,I#44=STIMER_CMPR5,"                  \
    "I#45=STIMER_CMPR6,I#46=STIMER_CMPR7,I#47=CLKGEN"

#endif /* __MCU_MCU_H_ */
