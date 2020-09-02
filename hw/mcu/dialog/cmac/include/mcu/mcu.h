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

#include "CMAC.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SVC_IRQ_NUMBER                  SVCall_IRQn

#define MCU_MEM_SYSRAM_START_ADDRESS    (0x20000000)
#define MCU_MEM_SYSRAM_END_ADDRESS      (0x20080000)

/* Map diagnostic signal to diagnostic port (output) */
#define MCU_DIAG_MAP(_port, _word, _evt)                                \
    CMAC->CM_DIAG_PORT ## _port ## _REG =                               \
        (_word << CMAC_CM_DIAG_PORT ## _port ## _REG_DIAG_WORD_Pos) |   \
        (CMAC_CM_DIAG_WORD ## _word ## _REG_DIAG ## _word ## _ ## _evt ## _Pos << CMAC_CM_DIAG_PORT ## _port ## _REG_DIAG_BIT_Pos)
#define MCU_DIAG_MAP_BIT(_port, _word, _evt, _bit)                      \
    CMAC->CM_DIAG_PORT ## _port ## _REG =                               \
        (_word << CMAC_CM_DIAG_PORT ## _port ## _REG_DIAG_WORD_Pos) |   \
        ((CMAC_CM_DIAG_WORD ## _word ## _REG_DIAG ## _word ## _ ## _evt ## _Pos + (_bit)) << CMAC_CM_DIAG_PORT ## _port ## _REG_DIAG_BIT_Pos)

/* Output diagnostic setial message */
#ifndef MCU_DIAG_SER_DISABLE
#define MCU_DIAG_SER(_ch) \
    CMAC->CM_DIAG_DSER_REG = _ch;
#else
#define MCU_DIAG_SER(_ch)
#endif

#define MCU_DIAG_GPIO0(_pin) \
    *(volatile uint32_t *)(0x50020A10) = 1 << (_pin)
#define MCU_DIAG_GPIO1(_pin) \
    *(volatile uint32_t *)(0x50020A08) = 1 << (_pin)

#ifdef __cplusplus
}
#endif

#endif /* __MCU_MCU_H_ */

