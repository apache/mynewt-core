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
#include <CMAC.h>
#include <mcu/cmac_pdc.h>
#include <mcu/cmac_timer.h>
#include <ipc_cmac/shm.h>
#include <ipc_cmac/mbox.h>
#include <ipc_cmac/rand.h>
#include "cmac_priv.h"

extern void ble_rf_calibrate_req(void);

void
SYS2CMAC_IRQHandler(void)
{
    uint16_t pending_ops;

    if (CMAC->CM_EXC_STAT_REG & CMAC_CM_EXC_STAT_REG_EXC_SYS2CMAC_Msk) {
        cmac_shm_lock();
        pending_ops = g_cmac_shm_ctrl.pending_ops;
        g_cmac_shm_ctrl.pending_ops = 0;
        cmac_shm_unlock();

        cmac_mbox_read();
        cmac_rand_read();

        if (pending_ops & CMAC_SHM_CB_PENDING_OP_SLEEP_UPDATE) {
            cmac_timer_slp_update(g_cmac_shm_ctrl.lp_clock_freq);
            cmac_sleep_wakeup_time_update(g_cmac_shm_ctrl.wakeup_lpclk_ticks);
        }

        if (pending_ops & CMAC_SHM_CB_PENDING_OP_RF_CAL) {
            ble_rf_calibrate_req();
        }

        CMAC->CM_EXC_STAT_REG = CMAC_CM_EXC_STAT_REG_EXC_SYS2CMAC_Msk;
    }

    cmac_pdc_ack_all();
}
