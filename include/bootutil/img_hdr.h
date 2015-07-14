#ifndef H_IMG_HDR_
#define H_IMG_HDR_

#define IMG_MAGIC   0x12345678

struct img_hdr {
    uint32_t magic;
    uint32_t crc32; /* Covers remainder of header and all of image body. */
    uint32_t hdr_size;
    uint32_t img_size; /* Does not include header. */
    /* XXX: version */
};

#endif

