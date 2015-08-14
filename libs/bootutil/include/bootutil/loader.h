#ifndef H_LOADER_
#define H_LOADER_

#include <inttypes.h>
struct ffs_area_desc;
struct image_header;

/** A request object instructing the boot loader how to proceed. */
struct boot_req {
    /**
     * Array of area descriptors indicating the layout of internal flash; must
     * be terminated with a 0-length element.
     */
    struct ffs_area_desc *br_area_descs;

    /**
     * Array of indices of elements in the br_area_descs array; indicates which
     * areas hold image data.
     */
    uint8_t *br_image_areas;

    /**
     * Array of indices of elements in the br_area_descs array; indicates which
     * areas represent the beginning of an image slot.  This should be a subset
     * of the br_image_areas array.
     */
    uint8_t *br_slot_areas;

    /**
     * The number of image areas (i.e., the size of the br_image_areas array).
     */
    uint8_t br_num_image_areas;

    /** The index of the area to use as the image scratch area. */
    uint8_t br_scratch_area_idx;
};

/**
 * A response object provided by the boot loader code; indicates where to jump
 * to execute the main image.
 */
struct boot_rsp {
    /** A pointer to the header of the image to be executed. */
    const struct image_header *br_hdr;

    /**
     * The flash offset of the image to execute.  Indicates the position of
     * the image header.
     */
    uint32_t br_image_addr;
};

int boot_go(const struct boot_req *req, struct boot_rsp *rsp);

#endif
