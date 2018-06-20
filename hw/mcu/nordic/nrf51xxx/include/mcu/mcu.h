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
 * Defines for naming GPIOs.
 */
#define MCU_GPIO_PORTA(pin)	((0 * 16) + (pin))

#define MCU_SYSVIEW_INTERRUPTS \
    "I#1=Reset,I#2=MNI,I#3=HardFault,I#11=SVCall,I#12=DebugMonitor,I#14=PendSV,I#15=SysTick," \
    "I#16=POWER_CLOCK,I#17=RADIO,I#18=UART0,I#19=SPI0_TWI0,I#20=SPI1_TWI1," \
    "I#22=GPIOTE,I#23=ADC,I#24=TIMER0,I#25=TIMER1,I#26=TIMER2,I#27=RTC0,I#28=TEMP," \
    "I#29=RNG,I#30=ECB,I#31=CCM_AAR,I#32=WDT,I#33=RTC1,I#34=QDEC,I#35=LPCOMP,I#36=SWI0," \
    "I#37=SWI1,I#38=SWI2,I#39=SWI3,I#40=SWI4,I#41=SWI5"

#ifdef __cplusplus
}
#endif

#endif /* __MCU_MCU_H_ */
