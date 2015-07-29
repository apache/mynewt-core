#ifndef H_IMG_HDR_
#define H_IMG_HDR_

#include <inttypes.h>

#define IMAGE_MAGIC         0x96f3b83c
#define IMAGE_MAGIC_NONE    0xffffffff

#define IMAGE_F_PIC     0x00000001

#define IMAGE_HEADER_CRC_OFFSET     4

struct image_version {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
    uint32_t iv_build_num;
};

struct image_header {
    uint32_t ih_magic;
    uint32_t ih_crc32; /* Covers remainder of header and all of image body. */
    uint32_t ih_hdr_size;
    uint32_t ih_img_size; /* Does not include header. */
    uint32_t ih_flags;
    struct image_version ih_ver;
};

#endif

