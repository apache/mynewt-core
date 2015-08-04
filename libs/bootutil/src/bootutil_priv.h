#ifndef H_BOOTUTIL_PRIV_
#define H_BOOTUTIL_PRIV_

#include "bootutil/image.h"
struct image_header;

#define BOOT_EFLASH     1
#define BOOT_EFILE      2
#define BOOT_EBADIMAGE  3
#define BOOT_EBADVECT   4
#define BOOT_EBADSTATUS 5
#define BOOT_ENOMEM     6

#define BOOT_IMAGE_NUM_NONE     0xff

#define BOOT_PATH_MAIN      "/boot/main"
#define BOOT_PATH_TEST      "/boot/test"
#define BOOT_PATH_STATUS    "/boot/status"

struct boot_status_entry {
    uint8_t bse_image_num;
    uint8_t bse_part_num;
};

struct boot_status {
    uint32_t bs_img1_length;
    uint32_t bs_img2_length;
    /* Followed by sequence of boot status entries. */
};

int boot_vect_read_test(struct image_version *out_ver);
int boot_vect_read_main(struct image_version *out_ver);
int boot_vect_delete_test(void);
int boot_vect_delete_main(void);
void boot_read_image_headers(struct image_header *out_headers,
                             const uint32_t *addresses, int num_addresses);
int boot_read_status(struct boot_status *out_status,
                     struct boot_status_entry *out_entries,
                     int num_areas);
int boot_write_status(const struct boot_status *status,
                      const struct boot_status_entry *entries,
                      int num_areas);
void boot_clear_status(void);

#endif

