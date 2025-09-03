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

#ifndef _GCM_MYNEWT_H_
#define _GCM_MYNEWT_H_

#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#include <mbedtls/gcm.h>

/**
 * Same as mbedtls_gcm_setkey, but with preallocated memory for cipher algorithm context
 */
int mbedtls_gcm_setkey_noalloc( mbedtls_gcm_context *ctx,
                                const mbedtls_cipher_info_t *cipher_info,
                                const unsigned char *key,
                                unsigned int keybits,
                                void *cipher_ctx);


#endif /* _GCM_MYNEWT_H_ */
