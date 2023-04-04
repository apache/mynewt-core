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

#include <os/mynewt.h>
#include <nrf_gpio.h>
#include <tfm/tfm.h>

int SECURE_CALL
tfm_uicr_otp_read(uint8_t n, uint32_t *ret)
{
    int err = 0;

    if (n >= 192) {
        err = TFM_ERR_INVALID_PARAM;
    } else if (n < MYNEWT_VAL(TFM_UICR_OTP_MIN_ADDR) || n > MYNEWT_VAL(TFM_UICR_OTP_MAX_ADDR)) {
        err = TFM_ERR_ACCESS_DENIED;
    } else {
        *ret = NRF_UICR_S->OTP[n];
    }

    return err;
}

int SECURE_CALL
tfm_uicr_protect_device(uint8_t *approtect, uint8_t *secure_approtect, uint8_t *erase_protect)
{
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
    if (approtect) {
        if (*approtect != 0 && NRF_UICR_S->APPROTECT == 0x50FA50FA) {
            NRF_UICR_S->APPROTECT = 0;
        }
        *approtect = NRF_UICR_S->APPROTECT == 0;
    }
    if (secure_approtect) {
        if (*secure_approtect != 0 && NRF_UICR_S->SECUREAPPROTECT == 0x50FA50FA) {
            NRF_UICR_S->SECUREAPPROTECT = 0;
        }
        *secure_approtect = NRF_UICR_S->SECUREAPPROTECT == 0;
    }
    if (erase_protect) {
        if (*erase_protect != 0 && NRF_UICR_S->ERASEPROTECT == 0xFFFFFFFF) {
            NRF_UICR_S->ERASEPROTECT = 0;
        }
        *erase_protect = NRF_UICR_S->ERASEPROTECT == 0;
    }
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;

    return 0;
}

int SECURE_CALL
tfm_gpio_pin_mcu_select(uint32_t pin_number, nrf_gpio_pin_mcusel_t mcu_sel)
{
    int err = 0;
    uint32_t pin_mask = (1u << (pin_number & 31));

    if (!nrf_gpio_pin_present_check(pin_number)) {
        err = TFM_ERR_INVALID_PARAM;
    } else if (((pin_number < 32) && (MYNEWT_VAL(TFM_MCU_SEL_GPIO0) & pin_mask)) ||
               ((pin_number >= 32) && (pin_number < 64) &&
                (MYNEWT_VAL(TFM_MCU_SEL_GPIO1) & pin_mask))) {
        nrf_gpio_pin_mcu_select(pin_number, mcu_sel);
    } else {
        err = TFM_ERR_ACCESS_DENIED;
    }

    return err;
}
