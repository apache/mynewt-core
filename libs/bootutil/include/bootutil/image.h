#ifndef H_IMG_HDR_
#define H_IMG_HDR_

#define IMG_MAGIC   0x96f3b83c

struct image_version {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
};

struct image_header {
    uint32_t magic;
    uint32_t crc32; /* Covers remainder of header and all of image body. */
    uint32_t hdr_size;
    uint32_t img_size; /* Does not include header. */
    struct image_version ih_ver;
};

#endif

