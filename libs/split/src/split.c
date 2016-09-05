
#include <assert.h>
#include <bootutil/bootutil_misc.h>
#include <bootutil/image.h>
#include <bootutil/loader.h>
#include <split/split.h>
#include <split/split_priv.h>


#define LOADER_IMAGE_SLOT    0
#define SPLIT_IMAGE_SLOT    1
#define SPLIT_TOTAL_IMAGES  2

void
split_app_init(void) {
    int rc;

    rc = split_conf_init();
    assert(rc==0);

    rc = split_nmgr_register();
    assert(rc==0);
}

splitStatus_t
split_check_status(void) {
    int rc;
    void *entry;
    rc =split_go(LOADER_IMAGE_SLOT, SPLIT_IMAGE_SLOT, &entry);

    if(rc == SPLIT_GO_ERR) {
        return SPLIT_INVALID;
    } else if (rc) {
    }
    return SPLIT_MATCHING;
}

/**
 * This validates and provides the loader image data
 *
 * @param req                   Contains information about the flash layout.
 * @param rsp                   On success, indicates how booting should occur.
 *
 * @return                      0 on success; nonzero on failure.
 */


int
split_app_go(void **entry, int toBoot) {
    splitMode_t split;
    int rc;

    if (toBoot) {
        /* if we can't read this, then we don't boot an app */
        rc = split_read_split(&split);
        if(rc) {
            return -1;
        }

        /* if we are told not to, then we don't boot an app */
        if(split == SPLIT_NONE) {
            return -1;
        }

        /* if this is a one-time test, reset the split mode */
        if (split == SPLIT_TEST) {
            split_write_split(SPLIT_NONE);
        }
    }

    rc =split_go(LOADER_IMAGE_SLOT, SPLIT_IMAGE_SLOT, entry);
    return rc;
}

