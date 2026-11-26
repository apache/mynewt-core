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

#ifndef __HAL_BSP_INT_H_
#define __HAL_BSP_INT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Below API shall be defined by BSP to provide provisioned data. It will be
 * called by generic hal_bsp_prov_data_get().
 */

int hal_bsp_prov_data_get_int(uint16_t id, void *data, uint16_t *length);

#ifdef __cplusplus
}
#endif

#endif
