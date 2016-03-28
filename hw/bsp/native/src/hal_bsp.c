/**
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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include "hal/hal_flash_int.h"
#include "mcu/native_bsp.h"
#include <bsp/bsp.h>
#include <bsp/bsp_sysid.h>
#include <mcu/hal_pwm.h>

const struct hal_flash *
bsp_flash_dev(uint8_t id)
{
    /*
     * Just one to start with
     */
    if (id != 0) {
        return NULL;
    }
    return &native_flash_dev;
}


struct hal_pwm *
bsp_get_hal_pwm_driver(enum system_device_id sysid) 
{
    switch(sysid) 
    {
        case NATIVE_BSP_PWM_0:
            return native_pwm_create (NATIVE_MCU_PWM0);
        case NATIVE_BSP_PWM_1:
            return native_pwm_create (NATIVE_MCU_PWM1);
        case NATIVE_BSP_PWM_2:
            return native_pwm_create (NATIVE_MCU_PWM2);
        case NATIVE_BSP_PWM_3:
            return native_pwm_create (NATIVE_MCU_PWM3);
        case NATIVE_BSP_PWM_4:
            return native_pwm_create (NATIVE_MCU_PWM4);
        case NATIVE_BSP_PWM_5:
            return native_pwm_create (NATIVE_MCU_PWM5);
        case NATIVE_BSP_PWM_6:
            return native_pwm_create (NATIVE_MCU_PWM6);
        case NATIVE_BSP_PWM_7:
            return native_pwm_create (NATIVE_MCU_PWM7);
        default:
            break;
    }
    return NULL;
}
