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

#include <mbedtls/gcm.h>

/**
 * \brief           This function feeds an input buffer into an ongoing GCM
 *                  encryption or decryption operation as additional data.
 *                  This needs to be called before starting enc/dec
 *                  operations.
 *
 *    `             The function expects input to be a multiple of 16
 *                  Bytes. Only the last call before mbedtls_gcm_update() or
 *                  mbedtls_gcm_finish() can be less than 16 Bytes.
 *
 *
 * \param ctx       The GCM context.
 * \param length    The length of the input data. This must be a multiple of
 *                  16 except in the last call before mbedtls_gcm_finish().
 * \param input     The buffer holding the input ADD.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_GCM_BAD_INPUT on failure.
 */
int mbedtls_gcm_update_add( mbedtls_gcm_context *ctx,
                            size_t length,
                            const unsigned char *input );


/**
 * Same as mbedtls_gcm_setkey, but with preallocated memory for cipher algorithm context
 */
int mbedtls_gcm_setkey_noalloc( mbedtls_gcm_context *ctx,
                                const mbedtls_cipher_info_t *cipher_info,
                                const unsigned char *key,
                                void *cipher_ctx);


#endif /* _GCM_MYNEWT_H_ */
