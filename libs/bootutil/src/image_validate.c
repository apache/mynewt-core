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

#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <hal/hal_flash.h>

#include <bootutil/image.h>

#include <mbedtls/sha256.h>

/*
 * Verify the integrity of the image.
 * Return non-zero if image could not be validated/does not validate.
 */
int
bootutil_img_validate(struct image_header *hdr, uint8_t flash_id, uint32_t addr,
  uint8_t *tmp_buf, uint32_t tmp_buf_sz)
{
    uint32_t blk_sz;
    uint32_t off;
    uint32_t size;
    mbedtls_sha256_context sha256_ctx;
    struct image_tlv tlv;
    uint8_t flash_hash[32];
    uint8_t computed_hash[32];
    int rc;

    if (hdr->ih_flags & IMAGE_F_HAS_SHA256) {
        /*
         * After image there's a TLV containing the hash.
         */
        mbedtls_sha256_init(&sha256_ctx);
        mbedtls_sha256_starts(&sha256_ctx, 0);
        size = hdr->ih_img_size + hdr->ih_hdr_size;
        for (off = 0; off < size; off += blk_sz) {
            blk_sz = size - off;
            if (blk_sz > tmp_buf_sz) {
                blk_sz = tmp_buf_sz;
            }
            rc = hal_flash_read(flash_id, addr + off, tmp_buf, blk_sz);
            if (rc) {
                return rc;
            }
            mbedtls_sha256_update(&sha256_ctx, tmp_buf, blk_sz);
        }
        mbedtls_sha256_finish(&sha256_ctx, computed_hash);

        size += hdr->ih_tlv_size;
        for (; off < size; off += sizeof(tlv) + tlv.it_len) {
            rc = hal_flash_read(flash_id, addr + off, &tlv, sizeof(tlv));
            if (rc) {
                return rc;
            }
            if (tlv.it_type == IMAGE_TLV_SHA256) {
                if (tlv.it_len != sizeof(flash_hash)) {
                    return -1;
                }
                rc = hal_flash_read(flash_id, addr + off + sizeof(tlv),
                  flash_hash, sizeof(flash_hash));
                if (rc) {
                    return rc;
                }
                if (memcmp(flash_hash, computed_hash, sizeof(flash_hash))) {
                    return -1;
                }
                return 0;
            }
        }
        /*
         * Header said there should hash TLV, no TLV found.
         */
        return -1;
    }
    return -1;
}
