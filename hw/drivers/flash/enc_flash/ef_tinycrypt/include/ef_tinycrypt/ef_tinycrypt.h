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

#ifndef __EF_TINYCRYPT_H__
#define __EF_TINYCRYPT_H__

/*
 * Encrypting flash driver using AES from Tinycrypt
 */
#include <enc_flash/enc_flash.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Tinycrypt specific version of the flash device.
 */
struct eflash_tinycrypt_dev {
    struct enc_flash_dev etd_dev;
    uint8_t etd_key[ENC_FLASH_BLK];
};

#ifdef __cplusplus
}
#endif

#endif /* __ENC_FLASH_TINYCRYPT_H__ */
