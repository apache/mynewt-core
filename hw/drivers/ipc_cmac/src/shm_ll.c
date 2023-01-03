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

#include <syscfg/syscfg.h>
#include <mcu/mcu.h>
#include <ipc_cmac/shm.h>

#define CMAC_SHM_MAGIC      __attribute__((section(".shm_magic")))
#define CMAC_SHM_VECT       __attribute__((section(".shm_vect")))
#define CMAC_SHM_DATA       __attribute__((section(".shm_data")))

#define FIELD_ARRAY_SIZE(t, f)  (sizeof(((t*)0)->f) / sizeof(((t*)0)->f[0]))

const struct cmac_shm_config g_cmac_shm_config = {
    .mbox_s2c_size = FIELD_ARRAY_SIZE(struct cmac_shm_ll_mbox_s2c, data),
    .mbox_c2s_size = FIELD_ARRAY_SIZE(struct cmac_shm_ll_mbox_c2s, data),
    .trim_rfcu_size = FIELD_ARRAY_SIZE(struct cmac_shm_ll_trim, rfcu),
    .trim_rfcu_mode1_size = FIELD_ARRAY_SIZE(struct cmac_shm_ll_trim, rfcu_mode1),
    .trim_rfcu_mode2_size = FIELD_ARRAY_SIZE(struct cmac_shm_ll_trim, rfcu_mode2),
    .trim_synth_size = FIELD_ARRAY_SIZE(struct cmac_shm_ll_trim, synth),
    .rand_size = FIELD_ARRAY_SIZE(struct cmac_shm_ll_rand, cmr_buf),
};

volatile struct cmac_shm_ctrl CMAC_SHM_DATA g_cmac_shm_ctrl;
volatile struct cmac_shm_ll_mbox_s2c CMAC_SHM_DATA g_cmac_shm_mbox_s2c;
volatile struct cmac_shm_ll_mbox_c2s CMAC_SHM_DATA g_cmac_shm_mbox_c2s;
volatile struct cmac_shm_ll_trim CMAC_SHM_DATA g_cmac_shm_trim;
volatile struct cmac_shm_ll_rand CMAC_SHM_DATA g_cmac_shm_rand;
volatile struct cmac_shm_dcdc CMAC_SHM_DATA g_cmac_shm_dcdc;
#if MYNEWT_VAL(CMAC_DEBUG_COREDUMP_ENABLE)
volatile struct cmac_shm_crashinfo CMAC_SHM_DATA g_cmac_shm_crashinfo;
#endif
#if MYNEWT_VAL(CMAC_DEBUG_DATA_ENABLE)
volatile struct cmac_shm_debugdata CMAC_SHM_DATA g_cmac_shm_debugdata;
#endif

const uint32_t g_cmac_shm_magic CMAC_SHM_MAGIC = CMAC_SHM_VECT_MAGIC;

const uint32_t g_cmac_shm_vect[] CMAC_SHM_VECT = {
    (uint32_t)&g_cmac_shm_config,
    (uint32_t)&g_cmac_shm_ctrl,
    (uint32_t)&g_cmac_shm_mbox_s2c,
    (uint32_t)&g_cmac_shm_mbox_c2s,
    (uint32_t)&g_cmac_shm_trim,
    (uint32_t)&g_cmac_shm_rand,
    (uint32_t)&g_cmac_shm_dcdc,
#if MYNEWT_VAL(CMAC_DEBUG_COREDUMP_ENABLE)
    (uint32_t)&g_cmac_shm_crashinfo,
#else
    0,
#endif
#if MYNEWT_VAL(CMAC_DEBUG_DATA_ENABLE)
    (uint32_t)&g_cmac_shm_debugdata,
#else
    0,
#endif
};

void
cmac_shm_ll_ready(void)
{
    g_cmac_shm_ctrl.magic = CMAC_SHM_CB_MAGIC;

    NVIC_SetPriority(SYS2CMAC_IRQn, 3);
    NVIC_EnableIRQ(SYS2CMAC_IRQn);
}
