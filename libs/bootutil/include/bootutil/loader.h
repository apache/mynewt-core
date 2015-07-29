#ifndef H_LOADER_
#define H_LOADER_

#include <inttypes.h>
struct ffs_area_desc;
struct image_header;

struct boot_req {
    struct ffs_area_desc *br_area_descs;
    uint32_t *br_image_addrs;
    uint16_t *br_image_areas;
    uint16_t br_scratch_area_idx;
    int br_num_image_areas;
    int br_num_slots;
};

struct boot_rsp {
    const struct image_header *br_hdr;
    uint32_t br_image_addr;
};

int boot_go(const struct boot_req *req, struct boot_rsp *rsp);

#endif
