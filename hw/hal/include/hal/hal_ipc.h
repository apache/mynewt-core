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

/**
 * @addtogroup HAL
 * @{
 *   @defgroup HALIPC HAL IPC
 *   @{
 */

#ifndef H_HAL_IPC_
#define H_HAL_IPC_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*hal_ipc_cb)(uint8_t channel);

void hal_ipc_init(void);
void hal_ipc_start(void);
void hal_ipc_register_callback(uint8_t channel, hal_ipc_cb cb);
void hal_ipc_enable_irq(uint8_t channel, bool enable);
int hal_ipc_signal(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* H_HAL_IPC_ */

/**
 *   @} HALIPC
 * @} HAL
 */
