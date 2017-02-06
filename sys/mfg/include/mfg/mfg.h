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

#ifndef H_MFG_
#define H_MFG_

#define MFG_EUNINIT                     1
#define MFG_EBADDATA                    2
#define MFG_EDONE                       3
#define MFG_EFLASH                      4

#define MFG_HASH_SZ                     32

#define MFG_META_TLV_TYPE_HASH          0x01
#define MFG_META_TLV_TYPE_FLASH_AREA    0x02

/** Informational only; not read by firmware. */
#define MFG_META_TLV_TYPE_FLASH_TRAITS  0x03

struct mfg_meta_tlv {
    uint8_t type;
    uint8_t size;
    /* Followed by packed data. */
};

struct mfg_meta_flash_area {
    uint8_t area_id;
    uint8_t device_id;
    uint16_t pad16;
    uint32_t offset;
    uint32_t size;
};

/** Informational only; not read by firmware. */
struct mfg_meta_flash_traits {
    uint8_t device_id;
    uint8_t min_write_sz;
};

int mfg_next_tlv(struct mfg_meta_tlv *tlv, uint32_t *off);
int mfg_next_tlv_with_type(struct mfg_meta_tlv *tlv, uint32_t *off,
                           uint8_t type);
int mfg_read_tlv_flash_area(const struct mfg_meta_tlv *tlv, uint32_t off,
                            struct mfg_meta_flash_area *out_mfa);
int mfg_read_tlv_hash(const struct mfg_meta_tlv *tlv, uint32_t off,
                      void *out_hash);
int mfg_init(void);

#endif
