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

#ifndef __MCU_CMAC_PDC_H_
#define __MCU_CMAC_PDC_H_

#include "CMAC.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MCU_PDC_CTRL_REGS_COUNT     16

static inline void
cmac_pdc_ack_all(void)
{
    int idx;

    for (idx = 0; idx < MCU_PDC_CTRL_REGS_COUNT; idx++) {
        if (PDC->PDC_PENDING_CMAC_REG & (1 << idx)) {
            PDC->PDC_ACKNOWLEDGE_REG = idx;
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif /* __MCU_CMAC_PDC_H_ */
