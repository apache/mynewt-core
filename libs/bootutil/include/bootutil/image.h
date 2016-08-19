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

#ifndef H_IMAGE_
#define H_IMAGE_

#include <inttypes.h>

#define IMAGE_MAGIC                 0x96f3b83c
#define IMAGE_MAGIC_NONE            0xffffffff

/*
 * Image header flags.
 */
#define IMAGE_F_PIC                 0x00000001
#define IMAGE_F_SHA256              0x00000002	/* Image contains hash TLV */
#define IMAGE_F_PKCS15_RSA2048_SHA256   0x00000004 /* PKCS15 w/RSA and SHA */
#define IMAGE_F_ECDSA224_SHA256     0x00000008  /* ECDSA256 over SHA256 */

#define IMAGE_HEADER_SIZE           32

/*
 * Image trailer TLV types.
 */
#define IMAGE_TLV_SHA256            1	/* SHA256 of image hdr and body */
#define IMAGE_TLV_RSA2048           2	/* RSA2048 of hash output */
#define IMAGE_TLV_ECDSA224          3   /* ECDSA of hash output */

struct image_version {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
    uint32_t iv_build_num;
};

#define IMAGE_SIZE(hdr)                                                 \
    ((hdr)->ih_tlv_size + (hdr)->ih_hdr_size + (hdr)->ih_img_size)

/** Image header.  All fields are in little endian byte order. */
struct image_header {
    uint32_t ih_magic;
    uint16_t ih_tlv_size; /* Trailing TLVs */
    uint8_t  ih_key_id;
    uint8_t  _pad1;
    uint16_t ih_hdr_size;
    uint16_t _pad2;
    uint32_t ih_img_size; /* Does not include header. */
    uint32_t ih_flags;
    struct image_version ih_ver;
    uint32_t _pad3;
};

/** Image trailer TLV format. All fields in little endian. */
struct image_tlv {
    uint8_t  it_type;
    uint8_t  _pad;
    uint16_t it_len;
};

_Static_assert(sizeof(struct image_header) == IMAGE_HEADER_SIZE,
               "struct image_header not required size");

int bootutil_img_validate(struct image_header *hdr, uint8_t flash_id,
  uint32_t addr, uint8_t *tmp_buf, uint32_t tmp_buf_sz);

#endif
