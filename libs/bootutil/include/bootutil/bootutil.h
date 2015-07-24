#ifndef H_BOOTUTIL_
#define H_BOOTUTIL_

#include "bootutil/image.h"
struct image_header;

#define BOOT_EFLASH     1
#define BOOT_EFILE      2
#define BOOT_EBADIMAGE  3
#define BOOT_EBADVECT   4

int boot_crc_is_valid(uint32_t addr, const struct image_header *hdr);
int boot_vect_read_cur(struct image_version *out_ver);
int boot_vect_rotate(void);
int boot_vect_repair(void);
void boot_read_image_headers(struct image_header *out_headers,
                             int *out_num_headers, const uint32_t *addresses,
                             int num_addresses);

#endif

