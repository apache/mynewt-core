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

#ifndef __IPC_CMAC_SHM_HS_H_
#define __IPC_CMAC_SHM_HS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CMAC_SHM_LOCK_VAL          0x40000000

extern volatile struct cmac_shm_config *g_cmac_shm_config;
extern volatile struct cmac_shm_ctrl *g_cmac_shm_ctrl;
extern volatile struct cmac_shm_mbox *g_cmac_shm_mbox_s2c;
extern volatile struct cmac_shm_mbox *g_cmac_shm_mbox_c2s;
extern volatile struct cmac_shm_dcdc *g_cmac_shm_dcdc;
extern volatile struct cmac_shm_trim *g_cmac_shm_trim;
extern volatile struct cmac_shm_rand *g_cmac_shm_rand;
extern volatile struct cmac_shm_crashinfo *g_cmac_shm_crashinfo;
extern volatile struct cmac_shm_debugdata *g_cmac_shm_debugdata;

void cmac_host_init(void);
void cmac_host_signal2cmac(void);

void cmac_host_req_sleep_update(void);
void cmac_host_rf_calibrate(void);

#ifdef __cplusplus
}
#endif

#endif /* __IPC_CMAC_SHM_HS_H_ */
