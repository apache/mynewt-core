#include <string.h>
#include <inttypes.h>
#include "bootutil/crc32.h"
#include "bootutil/img_hdr.h"

int
img_is_at(uint32_t addr, struct img_hdr *hdr)
{
    uint32_t hdr_size;
    uint32_t magic;

    memcpy(&magic, (void *)addr, sizeof magic);
    if (magic != IMG_MAGIC) {
        return 0;
    }

    memcpy(&hdr_size, (void *)(addr + sizeof magic), sizeof hdr_size);
    if (hdr_size > sizeof *hdr) {
        hdr_size = sizeof *hdr;
    }

    memset(hdr, 0, sizeof *hdr);
    memcpy(hdr, (void *)addr, hdr_size);

    return 1;
}

int
img_crc_is_valid(uint32_t addr, const struct img_hdr *hdr)
{
    uint32_t crc_off;
    uint32_t crc_len;
    uint32_t crc;

    crc_off = offsetof(struct img_hdr, crc32) + sizeof hdr->crc32;
    crc_len = hdr->hdr_size - crc_off + hdr->img_size;
    crc = crc32(0, (void *)(addr + crc_off), crc_len);

    return crc == hdr->crc32;
}

