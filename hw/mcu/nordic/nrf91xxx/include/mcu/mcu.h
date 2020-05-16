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

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Defines for naming GPIOs. NOTE: the nordic chip docs use numeric labels for
 * ports. Port A corresponds to Port 0, B to 1, etc. The nrf9160 has only one
 * port and thus uses pins 0 - 31.
 */
#define MCU_GPIO_PORTA(pin) ((0 * 31) + (pin))

#if NRF9160_XXAA

#define MCU_SYSVIEW_INTERRUPTS \
    "I#1=Reset,I#2=MNI,I#3=HardFault,I#4=MemoryMgmt,I#5=BusFault,I#6=UsageFault," \
    "I#11=SecureFault,I#12=SVCall,I#13=DebugMonitor,I#14=PendSV,I#15=SysTick," \
    "I#16=SPU,I#16=POWER_CLOCK,I#17=UARTE0_SPIM0_SPIS0_TWIM0_TWIS0,I#18=UARTE1_SPIM1_SPIS1_TWIM1_TWIS1," \
    "I#19=UARTE2_SPIM2_SPIS2_TWIM2_TWIS2,I#20=UARTE3_SPIM3_SPIS3_TWIM3_TWIS3,I#21=GPIOTE0,I#22=SAADC," \
    "I#23=TIMER0,I#24=TIMER1,I#25=TIMER2,I#26=RTC0,I#27=RTC1,I#28=WDT,I#29=EGU0,I#30=EGU1," \
    "I#31=EGU2,I#32=EGU3,I#33=EGU4,I#34=EGU5,I#35=PWM0,I#36=PWM1,I#37=PWM2,I#38=PWM3," \
    "I#39=PDM, I#40=I2S, I#41=IPC, I#42=FPU, I#43=GPIOTE1, I#44=KMU, I#58=CRYPTOCELL"

#endif

#ifdef __cplusplus
}
#endif

#endif /* __MCU_MCU_H_ */
