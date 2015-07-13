#ifndef H_BOOTPROJ_
#define H_BOOTPROJ_

int
img_is_at(uint32_t addr, struct img_hdr *hdr);

int
img_crc_is_valid(uint32_t addr, const struct img_hdr *hdr);

#endif

