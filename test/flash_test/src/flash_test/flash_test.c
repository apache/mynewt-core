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

#include <inttypes.h>
#include "os/mynewt.h"
#include <console/console.h>
#include <hal/hal_bsp.h>
#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#include <shell/shell.h>
#include <stdio.h>
#include <string.h>

static int flash_cli_cmd(int argc, char **argv);
static struct shell_cmd flash_cmd_struct = {
    .sc_cmd = "flash",
    .sc_cmd_func = flash_cli_cmd
};

static int
flash_cli_cmd(int argc, char **argv)
{
    const struct hal_flash *hf;
    uint32_t off = 0;
    uint32_t sz = 1;
    int sec_cnt;
    int i;
    int soff;
    char *eptr;
    char tmp_buf[8];
    char pr_str[80];

    hf = hal_bsp_flash_dev(0);
    if (!hf) {
        console_printf("No flash device present\n");
        return 0;
    }
    if (argc == 1) {
        /*
         * print status
         */
        console_printf("Flash at 0x%lx size 0x%lx with %d sectors,"
          " alignment req %d bytes\n",
                (long unsigned int) hf->hf_base_addr,
                (long unsigned int) hf->hf_size,
                hf->hf_sector_cnt,
                hf->hf_align);
        sec_cnt = hf->hf_sector_cnt;
        if (sec_cnt > 32) {
            sec_cnt = 32;
        }
        for (i = 0; i < sec_cnt; i++) {
            console_printf("  %d: %lx\n", i,
                    (long unsigned int) hal_flash_sector_size(hf, i));
        }
        if (sec_cnt != hf->hf_sector_cnt) {
            console_printf("...  %d: %lx\n", hf->hf_sector_cnt - 1,
              (long unsigned int) hal_flash_sector_size(hf, hf->hf_sector_cnt - 1));
        }
        return 0;
    }
    if (argc > 2) {
        off = strtoul(argv[2], &eptr, 0);
        if (*eptr != '\0') {
            console_printf("Invalid offset %s\n", argv[2]);
            goto err;
        }
    }
    if (argc > 3) {
        sz = strtoul(argv[3], &eptr, 0);
        if (*eptr != '\0') {
            console_printf("Invalid size %s\n", argv[3]);
            goto err;
        }
    }
    if (!strcmp(argv[1], "erase")) {
        console_printf("Erase 0x%lx + %lx\n",
                (long unsigned int) off, (long unsigned int) sz);

        if (hal_flash_erase(0, off, sz)) {
            console_printf("Flash erase failed\n");
        }
        console_printf("Done!\n");
    } else if (!strcmp(argv[1], "read")) {
        console_printf("Read 0x%lx + %lx\n",
                (long unsigned int) off, (long unsigned int) sz);
        sz += off;
        while (off < sz) {
            sec_cnt = min(sizeof(tmp_buf), sz - off);
            if (hal_flash_read(0, off, tmp_buf, sec_cnt)) {
                console_printf("flash read failure at %lx\n",
                        (long unsigned int) off);
                break;
            }
            for (i = 0, soff = 0; i < sec_cnt; i++) {
                soff += snprintf(pr_str + soff, sizeof(pr_str) - soff,
                  "0x%02x ", tmp_buf[i] & 0xff);
            }
            console_printf("  0x%lx: %s\n",
                    (long unsigned int) off, pr_str);
            off += sec_cnt;
        }
    } else if (!strcmp(argv[1], "write")) {
        console_printf("Write 0x%lx + %lx\n",
                (long unsigned int) off, (long unsigned int) sz);

        sz += off;
        for (i = 0; i < sizeof(tmp_buf); i++) {
            tmp_buf[i] = i + 1;
        }

        while (off < sz) {
            sec_cnt = min(sizeof(tmp_buf), sz - off);
            if (hal_flash_write(0, off, tmp_buf, sec_cnt)) {
                console_printf("flash write failure at %lx\n",
                        (long unsigned int) off);
            }
            off += sec_cnt;
        }
        console_printf("Done!\n");
    } else if ( !strcmp(argv[1], "?") || !strcmp(argv[1], "help"))
    {
        console_printf("Commands Available\n");
        console_printf("flash -- dumps sector map \n");
        console_printf("flash read <offset> <size> -- reads bytes from flash \n");
        console_printf("flash write <offset>  <size>  -- writes incrementing data pattern 0-8 to flash \n");
        console_printf("flash erase <offset> <size> -- erases flash \n");
    }
    return 0;
err:
    return -1;
}

/*
 * Initialize the package. Only called from sysinit().
 */
void
flash_test_init(void)
{
    shell_cmd_register(&flash_cmd_struct);
}
