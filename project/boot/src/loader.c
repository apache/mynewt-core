#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>

/* XXX: Where? */
#include "stm32f4xx/stm32f4xx.h"

#include "bootutil/img_hdr.h"

uint32_t __attribute__((section (".shared_section"))) __image_offset__;

struct image_desc {
    uint32_t addr;      /* Absolute address of image header. */
    uint32_t memrmp;    /* Value to assign the to the MEMRMP register. */
};

static const struct image_desc image_descs[] = {
    {
        .addr =     0x08004000,
        .memrmp =   0x00000000, /* Map internal flash to address 0. */
    },
    {
        .addr =     0x08080000,
        .memrmp =   0x00000000, /* Map internal flash to address 0. */
    },
};
#define NUM_IMAGE_DESCS ((int)(sizeof image_descs / sizeof image_descs[0]))

static void
img_jump(const struct image_desc *desc, const struct img_hdr *hdr)
{
    typedef void jump_fn(void);

    uint32_t base0entry;
    uint32_t img_start;
    uint32_t jump_addr;
    jump_fn *fn;

    img_start = desc->addr + hdr->hdr_size;

    /* First word contains initial MSP value. */
    __set_MSP(*(uint32_t *)img_start);

    /* Second word contains address of entry point (Reset_Handler). */
    base0entry = *(uint32_t *)(img_start + 4);
    jump_addr = base0entry + img_start - FLASH_BASE;
    fn = (jump_fn *)jump_addr;

    /* Remap memory such that image is at address 0. */
    SYSCFG->MEMRMP = desc->memrmp;
    __DSB();

    __image_offset__ = img_start - FLASH_BASE;

    /* Jump to image. */
    fn();
}

int
main(void)
{
    struct img_hdr hdr;
    int i;

    for (i = 0; i < NUM_IMAGE_DESCS; i++) {
        if (img_is_at(image_descs[i].addr, &hdr)) {
            if (img_crc_is_valid(image_descs[i].addr, &hdr)) {
                img_jump(image_descs + i, &hdr);
            }
        }
    }

    /* No valid image found. */
    while (1) { }
}

