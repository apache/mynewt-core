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

#ifndef __MCU_DA1469X_PD_H_
#define __MCU_DA1469X_PD_H_

#include <stdbool.h>
#include <stdint.h>
#include "DA1469xAB.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Available (controllable) power domains */
#define MCU_PD_DOMAIN_PER           0
#define MCU_PD_DOMAIN_RAD           1
#define MCU_PD_DOMAIN_TIM           2
#define MCU_PD_DOMAIN_COM           3

#define MCU_PD_DOMAIN_COUNT         4

int da1469x_pd_acquire(uint8_t pd);
int da1469x_pd_release(uint8_t pd);
int da1469x_pd_release_nowait(uint8_t pd);

#ifdef __cplusplus
}
#endif

#endif /* __MCU_DA1469X_PD_H_ */
