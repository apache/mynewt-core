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

#ifndef __MCU_DA1469X_PRAIL_H_
#define __MCU_DA1469X_PRAIL_H_

#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void da1469x_prail_initialize(void);

#if MYNEWT_VAL(MCU_DCDC_ENABLE)

/**
 * Enable DCDC
 */
void da1469x_prail_dcdc_enable(void);

/**
 * Restore DCDC settings
 *
 * This assumes DCDC was enabled before.
 */
void da1469x_prail_dcdc_restore(void);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __MCU_DA1469X_PRAIL_H_ */
