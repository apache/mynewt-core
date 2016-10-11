/**
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

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(IMGMGR_CLI)

#include <string.h>

#include <flash_map/flash_map.h>
#include <hal/hal_bsp.h>

#include <shell/shell.h>
#include <console/console.h>

#include <bootutil/image.h>
#include <bootutil/bootutil_misc.h>

#include <base64/hex.h>

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

static int imgr_cli_cmd(int argc, char **argv);

static struct shell_cmd shell_imgr_cmd = {
    .sc_cmd = "imgr",
    .sc_cmd_func = imgr_cli_cmd
};

static void
imgr_cli_show_slot(int slot)
{
    uint8_t hash[IMGMGR_HASH_LEN]; /* SHA256 hash */
    char hash_str[IMGMGR_HASH_LEN * 2 + 1];
    struct image_version ver;
    char ver_str[IMGMGR_NMGR_MAX_VER];
    uint32_t flags;

    if (imgr_read_info(slot, &ver, hash, &flags)) {
        return;
    }

    (void)imgr_ver_str(&ver, ver_str);

    console_printf("%8s: %s %c\n",
      ver_str, hex_format(hash, IMGMGR_HASH_LEN, hash_str, sizeof(hash_str)),
      flags & IMAGE_F_NON_BOOTABLE ? ' ' : 'b');
}

static void
imgr_cli_boot_get(void)
{
    int rc;
    int slot;

    /*
     * Display test image (if set)
     */
    rc = boot_vect_read_test(&slot);
    if (rc == 0) {
        imgr_cli_show_slot(slot);
    } else {
        console_printf("No test img set\n");
        return;
    }
}

static void
imgr_cli_boot_set(char *hash_str)
{
    uint8_t hash[IMGMGR_HASH_LEN];
    struct image_version ver;
    int rc;

    if (hex_parse(hash_str, strlen(hash_str), hash, sizeof(hash)) !=
      sizeof(hash)) {
        console_printf("Invalid hash %s\n", hash_str);
        return;
    }
    rc = imgr_find_by_hash(hash, &ver);
    if (rc < 0) {
        console_printf("Unknown img\n");
        return;
    }
    rc = boot_vect_write_test(rc);
    if (rc) {
        console_printf("Can't make img active\n");
        return;
    }
}

static int
imgr_cli_cmd(int argc, char **argv)
{
    int i;

    if (argc < 2) {
        console_printf("Too few args\n");
        return 0;
    }
    if (!strcmp(argv[1], "list")) {
        for (i = 0; i < 2; i++) {
            imgr_cli_show_slot(i);
        }
    } else if (!strcmp(argv[1], "boot")) {
        if (argc > 2) {
            imgr_cli_boot_set(argv[2]);
        } else {
            imgr_cli_boot_get();
        }
    } else if (!strcmp(argv[1], "ver")) {
        imgr_cli_show_slot(boot_current_slot);
    } else {
        console_printf("Unknown cmd\n");
    }
    return 0;
}

int
imgr_cli_register(void)
{
    return shell_cmd_register(&shell_imgr_cmd);
}
#endif /* MYNEWT_VAL(IMGMGR_CLI) */
