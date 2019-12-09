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

#ifndef __EF_DA1469X_H__
#define __EF_DA1469X_H__

/*
 * Encrypting flash driver for da1469x
 */
#include <enc_flash/enc_flash.h>
#include <os/mynewt.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * da1469x specific version of the flash device.
 */
struct eflash_da1469x_dev {
    struct enc_flash_dev end_dev;
    struct os_sem ef_sem;
};

#ifdef __cplusplus
}
#endif

#endif /* __EF_DA1469X_H__ */
