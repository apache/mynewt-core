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

#include <stdint.h>
#include "hal/hal_bsp.h"
#include "hal/hal_bsp_int.h"
#include "defs/error.h"

static hal_bsp_prov_data_cb g_hal_bsp_prov_data_cb;

int
hal_bsp_prov_data_get(uint16_t id, void *data, uint16_t *length)
{
    int rc;

    if (!data || !length) {
        return SYS_EINVAL;
    }

    /*
     * Check length for well-known data. Also adjust length so data providers
     * do not need to check this again.
     */
    switch (id) {
    case HAL_BSP_PROV_BLE_PUBLIC_ADDR:
    case HAL_BSP_PROV_BLE_STATIC_ADDR:
        if (*length < 6) {
            *length = 6;
            return SYS_ENOMEM;
        }
        *length = 6;
        break;
    case HAL_BSP_PROV_BLE_IRK:
        if (*length < 16) {
            *length = 16;
            return SYS_ENOMEM;
        }
        *length = 16;
        break;
    }

    if (g_hal_bsp_prov_data_cb) {
        rc = g_hal_bsp_prov_data_cb(id, data, length);
        if (rc != SYS_ENOTSUP) {
            return rc;
        }
    }

    return hal_bsp_prov_data_get_int(id, data, length);
}

int
hal_bsp_prov_data_set_cb(hal_bsp_prov_data_cb cb)
{
    if (!cb) {
        return SYS_EINVAL;
    }

    if (g_hal_bsp_prov_data_cb) {
        return SYS_EALREADY;
    }

    g_hal_bsp_prov_data_cb = cb;

    return 0;
}
