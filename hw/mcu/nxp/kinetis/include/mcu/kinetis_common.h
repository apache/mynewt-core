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

#ifndef __KINETIS_COMMON_H_
#define __KINETIS_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <fsl_device_registers.h>

#if MYNEWT_VAL(BSP_MK64F12)
#include "MK64F12.h"
#elif MYNEWT_VAL(BSP_MK80F)
#include "MK80F25615.h"
#elif MYNEWT_VAL(BSP_MK81F)
#include "MK81F25615.h"
#elif MYNEWT_VAL(BSP_MK82F)
#include "MK82F25615.h"
#else
#error "Unsupported MCU"
#endif
#ifdef __cplusplus
}
#endif

#endif /* __KINETIS_COMMON_H_ */

