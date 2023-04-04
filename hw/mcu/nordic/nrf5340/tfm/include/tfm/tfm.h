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
#include <os/mynewt.h>
#include <nrf_gpio.h>

#if (MYNEWT_VAL(BOOT_LOADER) && MYNEWT_VAL(TFM_EXPORT_NSC)) || \
    (!MYNEWT_VAL(BOOT_LOADER) && MYNEWT_VAL(MCU_APP_SECURE) && MYNEWT_VAL(TFM_EXPORT_NSC)) || \
    (!MYNEWT_VAL(BOOT_LOADER) && !MYNEWT_VAL(MCU_APP_SECURE))
#define SECURE_CALL __attribute__((cmse_nonsecure_entry))
#else
/* Whole application is secure */
#define SECURE_CALL
#endif

#define TFM_ERR_ACCESS_DENIED   SYS_EACCES
#define TFM_ERR_INVALID_PARAM   SYS_EINVAL

/**
 * Read UICR OTP word
 *
 * @param n   - word to read
 * @param ret - buffer for received value
 * @return 0 on success
 *         TFM_ERR_INVALID_PARAM - when n is out of range <0-191>
 *         TFM_ERR_ACCESS_DENIED - when n is denied by secure code
 */
int SECURE_CALL tfm_uicr_otp_read(uint8_t n, uint32_t *ret);

/**
 *
 * @param pin_number
 * @param mcu_sel
 * @return 0 on success
 *         TFM_ERR_INVALID_PARAM - when pin_number is not present on chip
 *         TFM_ERR_ACCESS_DENIED - when non-secure code is not allowed to change pin's MCU
 */
int SECURE_CALL tfm_gpio_pin_mcu_select(uint32_t pin_number, nrf_gpio_pin_mcusel_t mcu_sel);

/**
 * Function to set or read device protection status
 *
 * When variable pointer is non-NULL and variable is not 0 selected protection is activated.
 * When variable pointer is non-NULL but is set to 0 variable is updated with current
 * protection status.
 *
 * @param approtect - address of variable to set/read approtect status
 * @param secure_approtect - address of variable to set/read secure_approtect status
 * @param erase_protect -  - address of variable to set/read erase-all protection status
 * @return 0 on success
 */
int SECURE_CALL tfm_uicr_protect_device(uint8_t *approtect, uint8_t *secure_approtect, uint8_t *erase_protect);
