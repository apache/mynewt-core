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

#ifndef __IPC_CMAC_SHM_LL_H_
#define __IPC_CMAC_SHM_LL_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CMAC_SHM_LOCK_VAL           0xc0000000

#define CMAC_SHM_MBOX_S2C_SIZE      MYNEWT_VAL(CMAC_MBOX_SIZE_S2C)
#define CMAC_SHM_MBOX_C2S_SIZE      MYNEWT_VAL(CMAC_MBOX_SIZE_C2S)
#define CMAC_SHM_TRIM_RFCU_SIZE     MYNEWT_VAL(CMAC_TRIM_SIZE_RFCU)
#define CMAC_SHM_TRIM_RFCU1_SIZE    2
#define CMAC_SHM_TRIM_RFCU2_SIZE    2
#define CMAC_SHM_TRIM_SYNTH_SIZE    MYNEWT_VAL(CMAC_TRIM_SIZE_SYNTH)
#define CMAC_SHM_RAND_SIZE          16

struct cmac_shm_ll_mbox_s2c {
    uint16_t rd_off;
    uint16_t wr_off;
    uint8_t data[CMAC_SHM_MBOX_S2C_SIZE];
};

struct cmac_shm_ll_mbox_c2s {
    uint16_t rd_off;
    uint16_t wr_off;
    uint8_t data[CMAC_SHM_MBOX_C2S_SIZE];
};

struct cmac_shm_ll_trim {
    uint8_t rfcu_len;
    uint8_t rfcu_mode1_len;
    uint8_t rfcu_mode2_len;
    uint8_t synth_len;
    uint32_t rfcu[CMAC_SHM_TRIM_RFCU_SIZE];
    uint32_t rfcu_mode1[CMAC_SHM_TRIM_RFCU1_SIZE];
    uint32_t rfcu_mode2[CMAC_SHM_TRIM_RFCU2_SIZE];
    uint32_t synth[CMAC_SHM_TRIM_SYNTH_SIZE];
};

struct cmac_shm_ll_rand {
    uint16_t cmr_active;
    uint16_t cmr_in;
    uint16_t cmr_out;
    uint32_t cmr_buf[CMAC_SHM_RAND_SIZE];
};

extern volatile struct cmac_shm_ctrl g_cmac_shm_ctrl;
extern volatile struct cmac_shm_ll_mbox_s2c g_cmac_shm_mbox_s2c;
extern volatile struct cmac_shm_ll_mbox_c2s g_cmac_shm_mbox_c2s;
extern volatile struct cmac_shm_dcdc g_cmac_shm_dcdc;
extern volatile struct cmac_shm_ll_trim g_cmac_shm_trim;
extern volatile struct cmac_shm_ll_rand g_cmac_shm_rand;
#if MYNEWT_VAL(CMAC_DEBUG_COREDUMP_ENABLE)
extern volatile struct cmac_shm_crashinfo g_cmac_shm_crashinfo;
#endif
#if MYNEWT_VAL(CMAC_DEBUG_DATA_ENABLE)
extern volatile struct cmac_shm_debugdata g_cmac_shm_debugdata;
#endif

void cmac_shm_ll_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* __IPC_CMAC_SHM_LL_H_ */
