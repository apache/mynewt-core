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
#include <mbedtls/rsa.h>
#include <mbedtls/asn1.h>

#ifdef IMAGE_SIGNATURES
/*
 * XXX how to include public key in the build in a sane fashion? XXX
 */
#include "../../../image_sign_pub.c"
#define bootutil_key image_sign_pub_der2
#define bootutil_key_len image_sign_pub_der2_len

static const uint8_t sha256_oid[] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
    0x00, 0x04, 0x20
};
#endif

/*
 * Compute SHA256 over the image.
 */
static int
bootutil_img_hash(struct image_header *hdr, uint8_t flash_id, uint32_t addr,
  uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *hash_result)
{
    mbedtls_sha256_context sha256_ctx;
    uint32_t blk_sz;
    uint32_t size;
    uint32_t off;
    int rc;

    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);

    size = hdr->ih_img_size + hdr->ih_hdr_size;

    /*
     * Hash is computed over image header and image itself. No TLV is
     * included ATM.
     */
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
    mbedtls_sha256_finish(&sha256_ctx, hash_result);

    return 0;
}

#ifdef IMAGE_SIGNATURES
/*
 * Parse the public key used for signing. Simple RSA format.
 */
static int
bootutil_parse_rsakey(mbedtls_rsa_context *ctx, uint8_t **p, uint8_t *end)
{
    int rc;
    size_t len;

    if ((rc = mbedtls_asn1_get_tag(p, end, &len,
          MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return -1;
    }

    if (*p + len != end) {
        return -2;
    }

    if ((rc = mbedtls_asn1_get_mpi(p, end, &ctx->N)) != 0 ||
      (rc = mbedtls_asn1_get_mpi(p, end, &ctx->E)) != 0) {
        return -3;
    }

    if (*p != end) {
        return -4;
    }

    if ((rc = mbedtls_rsa_check_pubkey(ctx)) != 0) {
        return -5;
    }

    ctx->len = mbedtls_mpi_size(&ctx->N);

    return 0;
}

/*
 * PKCS1.5 using RSA2048 computed over SHA256.
 */
static int
bootutil_cmp_rsasig(mbedtls_rsa_context *ctx, uint8_t *hash, uint32_t hlen,
  uint8_t *sig)
{
    uint8_t buf[MBEDTLS_MPI_MAX_SIZE];
    uint8_t *p;

    if (ctx->len != 256) {
        return -1;
    }

    if (mbedtls_rsa_public(ctx, sig, buf)) {
        return -1;
    }

    p = buf;

    if (*p++ != 0 || *p++ != MBEDTLS_RSA_SIGN) {
        return -1;
    }

    while (*p != 0) {
        if (p >= buf + ctx->len - 1 || *p != 0xFF) {
            return -1;
        }
        p++;
    }
    p++;

    if ((p - buf) + sizeof(sha256_oid) + hlen != ctx->len) {
        return -1;
    }

    if (memcmp(p, sha256_oid, sizeof(sha256_oid))) {
        return -1;
    }
    p += sizeof(sha256_oid);

    if (memcmp(p, hash, hlen)) {
        return -1;
    }

    return 0;
}

static int
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, int slen)
{
    mbedtls_rsa_context ctx;
    int rc;
    uint8_t *cp;
    uint8_t *end;

    mbedtls_rsa_init(&ctx, 0, 0);

    cp = (uint8_t *)bootutil_key;
    end = cp + bootutil_key_len;

    rc = bootutil_parse_rsakey(&ctx, &cp, end);
    if (rc || slen != ctx.len) {
        mbedtls_rsa_free(&ctx);
        return rc;
    }
    rc = bootutil_cmp_rsasig(&ctx, hash, hlen, sig);
    mbedtls_rsa_free(&ctx);

    return rc;
}
#endif

/*
 * Verify the integrity of the image.
 * Return non-zero if image could not be validated/does not validate.
 */
int
bootutil_img_validate(struct image_header *hdr, uint8_t flash_id, uint32_t addr,
  uint8_t *tmp_buf, uint32_t tmp_buf_sz)
{
    uint32_t off;
    uint32_t size;
    uint32_t sha_off = 0;
#ifdef IMAGE_SIGNATURES
    uint32_t rsa_off = 0;
#endif
    struct image_tlv tlv;
    uint8_t buf[256];
    uint8_t hash[32];
    int rc;

#ifdef IMAGE_SIGNATURES
    if ((hdr->ih_flags & IMAGE_F_PKCS15_RSA2048_SHA256) == 0) {
        return -1;
    }
#else
    if ((hdr->ih_flags & IMAGE_F_SHA256) == 0) {
        return -1;
    }
#endif

    rc = bootutil_img_hash(hdr, flash_id, addr, tmp_buf, tmp_buf_sz, hash);
    if (rc) {
        return rc;
    }

    /*
     * After image there's TLVs.
     */
    off = hdr->ih_img_size + hdr->ih_hdr_size;
    size = off + hdr->ih_tlv_size;

    for (; off < size; off += sizeof(tlv) + tlv.it_len) {
        rc = hal_flash_read(flash_id, addr + off, &tlv, sizeof(tlv));
        if (rc) {
            return rc;
        }
        if (tlv.it_type == IMAGE_TLV_SHA256) {
            if (tlv.it_len != sizeof(hash)) {
                return -1;
            }
            sha_off = addr + off + sizeof(tlv);
        }
#ifdef IMAGE_SIGNATURES
        if (tlv.it_type == IMAGE_TLV_RSA2048) {
            if (tlv.it_len != 256) { /* 2048 bits */
                return -1;
            }
            rsa_off = addr + off + sizeof(tlv);
        }
#endif
    }
    if (hdr->ih_flags & IMAGE_F_SHA256) {
        if (!sha_off) {
            /*
             * Header said there should be hash TLV, no TLV found.
             */
            return -1;
        }
        rc = hal_flash_read(flash_id, sha_off, buf, sizeof(hash));
        if (rc) {
            return rc;
        }
        if (memcmp(hash, buf, sizeof(hash))) {
            return -1;
        }
    }
#ifdef IMAGE_SIGNATURES
    if (hdr->ih_flags & IMAGE_F_PKCS15_RSA2048_SHA256) {
        if (!rsa_off) {
            /*
             * Header said there should be PKCS1.v5 signature, no TLV
             * found.
             */
            return -1;
        }
        rc = hal_flash_read(flash_id, rsa_off, buf, 256);
        if (rc) {
            return -1;
        }
        rc = bootutil_verify_sig(hash, sizeof(hash), buf, 256);
        if (rc) {
            return -1;
        }
    }
#endif
    return 0;
}
