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
#ifndef __ENC_FLASH_INT_H_
#define __ENC_FLASH_INT_H_

/**
 * Platform specific driver init.
 */
int enc_flash_init_arch(struct enc_flash_dev *edev);

/**
 * Platform specific key setup.
 */
void enc_flash_setkey_arch(struct enc_flash_dev *edev, uint8_t *key);

/*
 * Platform specific encrypt/decrypt function.
 */
void enc_flash_crypt_arch(struct enc_flash_dev *edev, uint32_t blk_addr,
                          const uint8_t *src, uint8_t *tgt, int off, int cnt);

#endif
