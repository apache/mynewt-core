#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include "hal/hal_flash.h"
#include "ffs/ffs.h"
#include "bootutil/bootutil.h"

/* XXX: Where? */
#include "stm32f4xx/stm32f4xx.h"

static const struct ffs_sector_desc loader_sector_descs[] = {
    { 0x08000000, 16 * 1024 },
    { 0x08004000, 16 * 1024 },
    { 0x08008000, 16 * 1024 },
    { 0x0800c000, 16 * 1024 },
    { 0x08010000, 64 * 1024 },
    { 0x08020000, 128 * 1024 },
    { 0x08040000, 128 * 1024 },
    { 0x08060000, 128 * 1024 },
    { 0x08080000, 128 * 1024 },
    { 0x080a0000, 128 * 1024 },
    { 0x080c0000, 128 * 1024 },
    { 0x080e0000, 128 * 1024 },
    { 0, 0 },
};

/** Image slots. */
static const uint32_t boot_img_addrs[2] = {
    0x08020000,
    0x08080000,
};

static struct image_header boot_img_hdrs[2];
static int boot_num_hdrs;

static void
boot_jump(const struct image_header *hdr)
{
    typedef void jump_fn(void);

    uint32_t base0entry;
    uint32_t img_start;
    uint32_t jump_addr;
    jump_fn *fn;

    img_start = boot_img_addrs[0] + hdr->hdr_size;

    /* First word contains initial MSP value. */
    __set_MSP(*(uint32_t *)img_start);

    /* Second word contains address of entry point (Reset_Handler). */
    base0entry = *(uint32_t *)(img_start + 4);
    jump_addr = base0entry;
    fn = (jump_fn *)jump_addr;

    /* Remap memory such that flash gets mapped to the code region. */
    SYSCFG->MEMRMP = 0;
    __DSB();

    /* Jump to image. */
    fn();
}

static int
boot_select_image_slot(void)
{
    struct image_version cur_ver;
    int rc;
    int i;

    rc = boot_vect_read_cur(&cur_ver);
    if (rc == 0) {
        for (i = 0; i < 2; i++) {
            if (memcmp(&boot_img_hdrs[i].ih_ver, &cur_ver,
                       sizeof cur_ver) == 0) {
                return i;
            }
        }
    }

    return -1;
}

int
main(void)
{
    const struct image_header *boot_hdr;
    int slot;
    int rc;

    rc = flash_init();
    assert(rc == 0);

    boot_read_image_headers(boot_img_hdrs, &boot_num_hdrs, boot_img_addrs, 2);

    rc = ffs_init();
    assert(rc == 0);

    /* Look for an ffs file system in internal flash. */
    ffs_detect(loader_sector_descs);

    slot = boot_select_image_slot();
    if (slot == -1) {
        /* Current image is not in flash.  Fall back to last-known-good. */
        boot_vect_repair();
        slot = boot_select_image_slot();
        if (slot == -1) {
            /* Last-known-good isn't present either.  Just boot from the first
             * image slot.
             */
            slot = 0;
        }
    }

    switch (slot) {
    case 0:
        boot_hdr = &boot_img_hdrs[0];
        break;

    case 1:
        /* XXX: Perform copy. */
        boot_vect_rotate();
        boot_hdr = &boot_img_hdrs[1];
        break;

    default:
        assert(0);
        break;
    }

    boot_jump(boot_hdr);

    return 0;
}

