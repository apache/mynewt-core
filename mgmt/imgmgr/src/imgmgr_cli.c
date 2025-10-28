/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "os/mynewt.h"

#if MYNEWT_VAL(IMGMGR_CLI)

#include <string.h>

#include <flash_map/flash_map.h>
#include <hal/hal_bsp.h>

#include <shell/shell.h>

#include <bootutil/image.h>
#include <bootutil/bootutil.h>

#include <base64/hex.h>

#include <parse/parse.h>

#include "imgmgr/imgmgr.h"
#include "img_mgmt/img_mgmt.h"
#include "imgmgr_priv.h"

static int imgr_cli_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                        struct streamer *streamer);

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_cmd_help imgr_cli_help = {
   .summary = "image management command",
   .usage = "\n"
            "    imgr list\n"
            "    imgr test <slot | hash>\n"
            "    imgr confirm [slot | hash]"
};
#endif

static void
imgr_cli_too_few_args(struct streamer *streamer)
{
    streamer_printf(streamer, "Too few args\n");
}

static const char *
imgr_cli_flags_str(uint32_t image_flags, uint8_t state_flags)
{
    static char buf[8];
    char *p;

    memset(buf, ' ', sizeof buf);
    p = buf;

    if (state_flags & IMG_MGMT_STATE_F_ACTIVE) {
        *p = 'a';
    }
    p++;
    if (!(image_flags & IMAGE_F_NON_BOOTABLE)) {
        *p = 'b';
    }
    p++;
    if (state_flags & IMG_MGMT_STATE_F_CONFIRMED) {
        *p = 'c';
    }
    p++;
    if (state_flags & IMG_MGMT_STATE_F_PENDING) {
        *p = 'p';
    }
    p++;

    *p = '\0';

    return buf;
}

static void
imgr_cli_show_slot(int slot, struct streamer *streamer)
{
    uint8_t hash[IMGMGR_HASH_LEN]; /* SHA256 hash */
    char hash_str[IMGMGR_HASH_LEN * 2 + 1];
    struct image_version ver;
    char ver_str[IMGMGR_NMGR_MAX_VER];
    uint32_t flags;
    uint8_t state_flags;

    if (img_mgmt_read_info(slot, &ver, hash, &flags)) {
        return;
    }

    state_flags = img_mgmt_state_flags(slot);

    (void)img_mgmt_ver_str(&ver, ver_str);

    streamer_printf(streamer, "%d %8s: %s %s\n",
      slot, ver_str,
      hex_format(hash, IMGMGR_HASH_LEN, hash_str, sizeof(hash_str)),
      imgr_cli_flags_str(flags, state_flags));
}

static int
imgr_cli_hash_parse(const char *arg, int *out_slot)
{
    uint8_t hash[IMGMGR_HASH_LEN];
    struct image_version ver;
    int slot;
    int rc;

    rc = hex_parse(arg, strlen(arg), hash, sizeof hash);
    if (rc != sizeof hash) {
        return SYS_EINVAL;
    }

    slot = imgr_find_by_hash(hash, &ver);
    if (slot == -1) {
        return SYS_ENOENT;
    }

    *out_slot = slot;
    return 0;
}

static int
imgr_cli_slot_or_hash_parse(const char *arg, int *out_slot, struct streamer *streamer)
{
    int rc;

    /* First, parse argument as a slot number.  Parts of the system assume slot
     * is 0 or 1; enforce those bounds here.
     */
    *out_slot = parse_ll_bounds(arg, 0, 1, &rc);
    if (rc == 0) {
        return 0;
    }

    /* Try to parse as a hash string. */
    rc = imgr_cli_hash_parse(arg, out_slot);
    switch (rc) {
    case 0:
        return 0;

    case SYS_ENOENT:
        streamer_printf(streamer, "No image with hash: %s\n", arg);
        return rc;

    default:
        streamer_printf(streamer, "Invalid slot number or image hash: %s\n", arg);
        return rc;
    }
}

static void
imgr_cli_set_pending(char *arg, int permanent, struct streamer *streamer)
{
    int slot;
    int rc;

    /* Parts of the system assume slot is 0 or 1; enforce here. */
    rc = imgr_cli_slot_or_hash_parse(arg, &slot, streamer);
    if (rc != 0) {
        return;
    }

    rc = img_mgmt_state_set_pending(slot, permanent);
    if (rc) {
        streamer_printf(streamer, "Error setting slot %d to pending; rc=%d\n", slot, rc);
        return;
    }
}

static void
imgr_cli_confirm(struct streamer *streamer)
{
    int rc;

    rc = img_mgmt_state_confirm();
    if (rc != 0) {
        streamer_printf(streamer, "Error confirming image state; rc=%d\n", rc);
        return;
    }
}

static void
imgr_cli_erase(struct streamer *streamer)
{
    const struct flash_area *fa;
    int area_id;
    int rc;

    area_id = imgmgr_find_best_area_id();
    if (area_id >= 0) {
        rc = flash_area_open(area_id, &fa);
        if (rc) {
            streamer_printf(streamer, "Error opening area %d\n", area_id);
            return;
        }
        rc = flash_area_erase(fa, 0, fa->fa_size);
        flash_area_close(fa);
        if (rc) {
            streamer_printf(streamer, "Error erasing area rc=%d\n", rc);
        }
    } else {
        /*
         * No slot where to erase!
         */
        streamer_printf(streamer, "No suitable area to erase\n");
    }
}

static int
imgr_cli_cmd(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *streamer)
{
    int i;

    if (argc < 2) {
        imgr_cli_too_few_args(streamer);
        return 0;
    }
    if (!strcmp(argv[1], "list")) {
        for (i = 0; i < 2; i++) {
            imgr_cli_show_slot(i, streamer);
        }
    } else if (!strcmp(argv[1], "test")) {
        if (argc < 3) {
            imgr_cli_too_few_args(streamer);
            return 0;
        } else {
            imgr_cli_set_pending(argv[2], 0, streamer);
        }
    } else if (!strcmp(argv[1], "confirm")) {
        if (argc < 3) {
            imgr_cli_confirm(streamer);
        } else {
            imgr_cli_set_pending(argv[2], 1, streamer);
        }
    } else if (!strcmp(argv[1], "erase")) {
        imgr_cli_erase(streamer);
    } else {
        streamer_printf(streamer, "Unknown cmd\n");
    }
    return 0;
}

MAKE_SHELL_EXT_CMD(imgr, imgr_cli_cmd, &imgr_cli_help)

#endif /* MYNEWT_VAL(IMGMGR_CLI) */
