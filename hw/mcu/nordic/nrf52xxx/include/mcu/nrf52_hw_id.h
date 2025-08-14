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

#ifndef H_NRF52_HW_ID_
#define H_NRF52_HW_ID_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int nrf52_hw_id_len(void);
int nrf52_hw_id(uint8_t *id, int max_len);

#ifdef __cplusplus
}
#endif

#endif  /* H_NRF52_HW_ID_ */
