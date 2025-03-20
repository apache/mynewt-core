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
#include "mbedtls_test.h"

#define AES_BLK_SZ 16

static const mbedtls_cipher_info_t *rsm_ucast_cipher;

/* This contains both ADD and plaintext for encryption */
static const uint8_t initial_data[110] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x11,
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x21, 0x22,
    0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x41, 0x42, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x51, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
    0x89, 0x8A, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
    0x9A, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA
};

static const uint8_t key[32] = { 0xC0, 0xCA, 0xC0, 0x1A, 0xC0, 0xCA, 0xC0,
                                 0x1A, 0xC0, 0xCA, 0xC0, 0x1A, 0xC0, 0xCA,
                                 0xC0, 0x1A, 0xC0, 0xCA, 0xC0, 0x1A, 0xC0,
                                 0xCA, 0xC0, 0x1A, 0xC0, 0xCA, 0xC0, 0x1A,
                                 0xC0, 0xCA, 0xC0, 0x1A };

static const uint8_t iv[12] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5,
                                0x6, 0x7, 0x8, 0x9, 0xA, 0xB };

static const uint8_t expected_tag[16] = { 0x05, 0x5D, 0x8E, 0xD4, 0xF9, 0x2A,
                                          0x87, 0x87, 0x6F, 0x23, 0xF2, 0xE6,
                                          0xF0, 0x1D, 0x6D, 0x5C };

static uint8_t test_tag[16];
static uint8_t test_buf[110];

static int
mbedtls_gcm_mynewt_test_crypt(uint8_t enc)
{
    int add_len = 40;
    mbedtls_gcm_context ctx;
    mbedtls_aes_context aes_ctx;
    uint8_t *ptr;

    uint16_t off;
    uint16_t blklen;
    uint16_t totlen;
    int rc;

    if (rsm_ucast_cipher == NULL) {
        rsm_ucast_cipher = mbedtls_cipher_info_from_values(MBEDTLS_CIPHER_ID_AES,
                                                           256, MBEDTLS_MODE_ECB);
    }

    memset(&ctx, 0, sizeof(ctx));
    mbedtls_aes_init(&aes_ctx);
    rc = mbedtls_gcm_setkey_noalloc(&ctx, rsm_ucast_cipher, key, &aes_ctx);
    if (rc) {
        goto out;
    }

    rc = mbedtls_gcm_starts(&ctx, enc == 1 ? MBEDTLS_GCM_ENCRYPT : MBEDTLS_GCM_DECRYPT,
                            iv, sizeof(iv), NULL, 0);
    if (rc) {
        goto out;
    }

    off = 0;
    totlen = 110;

    while (off < totlen) {
        ptr = test_buf + off;
        blklen = sizeof(test_buf) - off;
        if (blklen < AES_BLK_SZ) {
            blklen = AES_BLK_SZ;
        } else {
            blklen &= ~(AES_BLK_SZ - 1);
        }
        if (off < add_len) {
            if (blklen + off > add_len) {
                blklen = add_len - off;
            }
        } else {
            if (blklen + off > totlen) {
                blklen = totlen - off;
            }
        }

        if (off < add_len) {
            mbedtls_gcm_update_add(&ctx, blklen, ptr);
        } else {
            rc = mbedtls_gcm_update(&ctx, blklen, ptr, ptr);
            if (rc) {
                goto out;
            }
        }

        off += blklen;
    }

    rc = mbedtls_gcm_finish(&ctx, test_tag, sizeof(test_tag));
out:
    memset(&ctx, 0, sizeof(ctx));
    mbedtls_aes_free(&aes_ctx);
    if (rc) {
        return 1;
    }
    return 0;
}

TEST_CASE_SELF(gcm_mynewt_test)
{
    int rc;

    memcpy(test_buf, initial_data, sizeof(initial_data));

    rc = mbedtls_gcm_mynewt_test_crypt(1);
    TEST_ASSERT(rc == 0);

    rc = mbedtls_gcm_mynewt_test_crypt(0);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(memcmp(test_tag, expected_tag, sizeof(test_tag)) == 0);
    TEST_ASSERT(memcmp(test_buf, initial_data, sizeof(test_buf)) == 0);
}
