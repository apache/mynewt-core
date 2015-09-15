/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef H_IMAGE_
#define H_IMAGE_

#include <inttypes.h>

#define IMAGE_MAGIC                 0x96f3b83c
#define IMAGE_MAGIC_NONE            0xffffffff

#define IMAGE_F_PIC                 0x00000001

#define IMAGE_HEADER_CRC_OFFSET     4

#define IMAGE_HEADER_SIZE           28

struct image_version {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
    uint32_t iv_build_num;
};

/** Image header.  All fields are in little endian byte order. */
struct image_header {
    uint32_t ih_magic;
    uint32_t ih_crc32; /* Covers remainder of header and all of image body. */
    uint32_t ih_hdr_size;
    uint32_t ih_img_size; /* Does not include header. */
    uint32_t ih_flags;
    struct image_version ih_ver;
};

_Static_assert(sizeof(struct image_header) == IMAGE_HEADER_SIZE,
               "struct image_header not required size");

#endif
