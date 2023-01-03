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

#ifndef __IPC_CMAC_SHM_RAND_H_
#define __IPC_CMAC_SHM_RAND_H_

#include <syscfg/syscfg.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cmac_rand_isr_cb_t)(uint8_t rnum);
void cmac_rand_start(void);
void cmac_rand_stop(void);
void cmac_rand_read(void);
void cmac_rand_write(void);
void cmac_rand_chk_fill(void);
int cmac_rand_get_next(void);
int cmac_rand_is_active(void);
int cmac_rand_is_full(void);
void cmac_rand_fill(uint32_t *buf, int num_words);
void cmac_rand_set_isr_cb(cmac_rand_isr_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* __IPC_CMAC_SHM_RAND_H_ */
