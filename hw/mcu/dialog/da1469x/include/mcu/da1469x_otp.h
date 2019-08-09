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

#ifndef __MCU_DA1469X_OTP_H_
#define __MCU_DA1469X_OTP_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTP_ERR_INVALID_SIZE_ALIGNMENT -1
#define OTP_ERR_INVALID_ADDRESS -2
#define OTP_ERR_PROGRAM_VERIFY_FAILED -3

#define OTP_SEGMENT_CONFIG          0xc00
#define OTP_SEGMENT_QSPI_FW_KEYS    0xb00
#define OTP_SEGMENT_USER_DATA_KEYS  0xa00
#define OTP_SEGMENT_SIGNATURE_KEYS  0x8c0

int da1469x_otp_write(uint32_t address, const void *src,
                             uint32_t num_bytes);

int da1469x_otp_read(uint32_t address, void *dst, uint32_t num_bytes);

void da1469x_otp_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __MCU_DA1469X_OTP_H_ */
