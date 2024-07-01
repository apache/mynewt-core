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
#include <parse/parse.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static int flash_cli_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                         struct streamer *streamer);
static int flash_speed_test_cli(const struct shell_cmd *cmd, int argc,
                                char **argv, struct streamer *streamer);

static struct shell_cmd flash_cmd_struct =
    SHELL_CMD_EXT("flash", flash_cli_cmd, NULL);

static struct shell_cmd flash_speed_cli_struct =
    SHELL_CMD_EXT("flash_speed", flash_speed_test_cli, NULL);

static void
dump_sector_range_info(struct streamer *streamer, int start_sector, uint32_t start_address,
                       int sector_count, uint32_t sector_size)
{
    if (sector_count == 1) {
        streamer_printf(streamer, "  %d: 0x%lx\n", start_sector,
                        (long unsigned int)sector_size);
    } else {
        streamer_printf(streamer, "  %d-%d: 0x%lx (total 0x%x)\n", start_sector, start_sector + sector_count - 1,
                        (long unsigned int)sector_size, sector_size * sector_count);
    }
}

static int
flash_cli_cmd(const struct shell_cmd *cmd, int argc, char **argv,
              struct streamer *streamer)
{
    const struct hal_flash *hf;
    uint32_t off = 0;
    uint32_t sz = 1;
    uint32_t fa_off = 0;
    int sec_cnt;
    int i;
    int devid = 0;
    int rc;
    char *eptr;
    char tmp_buf[32];
    char pr_str[80];
    /* arg_idx points to first argument after 'flash' */
    int arg_idx = 1;

    if (argc > 1 && (!strcmp(argv[1], "?") || !strcmp(argv[1], "help"))) {
        streamer_printf(streamer, "Commands Available\n");
        streamer_printf(streamer, "flash [<id>] -- dumps sector map\n");
        streamer_printf(streamer, "flash [area] <id> read <offset> <size> -- reads bytes from flash\n");
        streamer_printf(streamer,
                        "flash [area] <id> write <offset> <size> -- writes incrementing data pattern 0-8 to flash\n");
        streamer_printf(streamer, "flash [area] <id> erase <offset> <size> -- erases flash\n");
        streamer_printf(streamer, "flash area -- shows flash areas\n");
        return 0;
    }

    arg_idx += (argc > 1 && strcmp(argv[1], "area") == 0) ? 1 : 0;
    /* If first argument was in fact 'area' */
    if (arg_idx == 2) {
        /* There was no more arguments, dump flash areas */
        if (argc == 2) {
            streamer_printf(streamer, "AreaID FlashId     Offset     Size\n");
            for (i = 0; i < ARRAY_SIZE(sysflash_map_dflt); ++i) {
                streamer_printf(streamer, "%6u %7u 0x%08x 0x%06x\n", sysflash_map_dflt[i].fa_id,
                                sysflash_map_dflt[i].fa_device_id,
                                (unsigned)sysflash_map_dflt[i].fa_off,
                                (unsigned)sysflash_map_dflt[i].fa_size);
            }
            return 0;
        } else {
            /* Try to parese flash area id */
            int id = (int)parse_ull_bounds(argv[arg_idx], 0, 255, &rc);
            const struct flash_area *fa;

            if (rc == 0) {
                rc = flash_area_open(id, &fa);
            }
            if (rc) {
                streamer_printf(streamer, "Invalid flash area id %s\n", argv[arg_idx]);
                return 0;
            }
            /* Keep information about flash area which will modify flash access place */
            fa_off = fa->fa_off;
            devid = fa->fa_device_id;
        }
    } else {
        /* Try parse flash id if any, if not set 0 will be used */
        if (argc == 2) {
            devid = (int)parse_ull_bounds(argv[1], 0, 255, &rc);
            if (rc != 0) {
                streamer_printf(streamer, "Invalid flash id %s\n", argv[1]);
                return 0;
            }
        }
    }
    arg_idx++;

    if (arg_idx >= argc) {
        do {
            hf = hal_bsp_flash_dev(devid);
            if (!hf) {
                if (argc == 2) {
                    streamer_printf(streamer, "Flash device not present\n");
                }
                return 0;
            }
            streamer_printf(streamer, "Flash %d start address %#lx size %#lx with %d "
                                      "sectors, alignment req %d bytes\n",
                    devid,
                    (long unsigned int) hf->hf_base_addr,
                    (long unsigned int) hf->hf_size,
                    hf->hf_sector_cnt,
                    hf->hf_align);
            sec_cnt = hf->hf_sector_cnt;
            uint32_t range_start;
            uint32_t range_sector_size = 0;
            int range_first_sector = -1;
            uint32_t sector_start_address;
            uint32_t sector_size;

            for (i = 0; i <= sec_cnt; ++i) {
                if (i < sec_cnt) {
                    hal_flash_sector_info(devid, i, &sector_start_address, &sector_size);
                    if (range_first_sector < 0) {
                        range_first_sector = i;
                        range_sector_size = sector_size;
                        range_start = sector_start_address;
                    }
                } else {
                    sector_start_address = 0;
                    sector_size = 0;
                }
                if (sector_size != range_sector_size && range_sector_size > 0) {
                    dump_sector_range_info(streamer, range_first_sector, range_start,
                                           i - range_first_sector, range_sector_size);
                    range_first_sector = i;
                    range_sector_size = sector_size;
                }
            }
            ++devid;
        } while (argc == 1);

        return 0;
    }

    if (arg_idx + 1 < argc) {
        off = strtoul(argv[arg_idx + 1], &eptr, 0);
        if (*eptr != '\0') {
            streamer_printf(streamer, "Invalid offset %s\n", argv[2]);
            goto err;
        }
    }
    if (arg_idx + 2 < argc) {
        sz = strtoul(argv[arg_idx + 2], &eptr, 0);
        if (*eptr != '\0') {
            streamer_printf(streamer, "Invalid size %s\n", argv[3]);
            goto err;
        }
    }
    if (!strcmp(argv[arg_idx], "erase")) {
        streamer_printf(streamer, "Erase %#lx + %#lx\n",
                (long unsigned int) off, (long unsigned int) sz);

        if (hal_flash_erase(devid, fa_off + off, sz)) {
            streamer_printf(streamer, "Flash erase failed\n");
        }
        streamer_printf(streamer, "Done!\n");
    } else if (!strcmp(argv[arg_idx], "read")) {
        streamer_printf(streamer, "Read %#lx + %#lx\n",
                (long unsigned int) off, (long unsigned int) sz);
        sz += off;
        while (off < sz) {
            sec_cnt = min(sizeof(tmp_buf), sz - off);
            if (hal_flash_read(devid, fa_off + off, tmp_buf, sec_cnt)) {
                streamer_printf(streamer, "flash read failure at %#lx\n",
                        (long unsigned int) off);
                break;
            }
            for (i = 0; i < sec_cnt; i++) {
                int n = i & 7;
                if (n == 0) {
                    streamer_printf(streamer, "  %#lx: ", (long unsigned int)off + i);
                }
                snprintf(pr_str + n * 5, 6, "0x%02x ", tmp_buf[i] & 0xff);
                pr_str[41 + n] = isprint((uint8_t)tmp_buf[i]) ? tmp_buf[i] : '.';
                if (n == 7 || i + 1 == sec_cnt) {
                    pr_str[41 + n + 1] = '\0';
                    memset(pr_str + (5 * n + 5), ' ', 5 * (7 - n) + 1);
                    streamer_printf(streamer, "%s\n", pr_str);
                }
            }
            off += sec_cnt;
        }
    } else if (!strcmp(argv[arg_idx], "write")) {
        streamer_printf(streamer, "Write %#lx + %#lx\n",
                (long unsigned int) off, (long unsigned int) sz);

        sz += off;
        for (i = 0; i < sizeof(tmp_buf); i++) {
            tmp_buf[i] = i + 1;
        }

        while (off < sz) {
            sec_cnt = min(sizeof(tmp_buf), sz - off);
            if (hal_flash_write(devid, fa_off + off, tmp_buf, sec_cnt)) {
                streamer_printf(streamer, "flash write failure at %#lx\n",
                        (long unsigned int) off);
            }
            off += sec_cnt;
        }
        streamer_printf(streamer, "Done!\n");
    }
    return 0;
err:
    return -1;
}


/*
 * Returns # of ops done within 2 seconds.
 */
int
flash_speed_test(int flash_dev, uint32_t addr, int sz, int move)
{
    int rc;
    int cnt = 0;
    int off = 0;
    int start_time;
    int end_time;
    void *data_buf;

    data_buf = malloc(sz);
    if (!data_buf) {
        return -1;
    }

    /*
     * Catch start of a tick.
     */
    start_time = os_time_get();
    while (1) {
        end_time = os_time_get();
        if (end_time != start_time) {
            start_time = end_time;
            break;
        }
    }

    /*
     * Measure for 2 secs.
     */
    do {
        rc = hal_flash_read(flash_dev, addr + off, data_buf, sz);
        if (rc) {
            console_printf("hal_flash_read(%d, 0x%x, %d) = %d\n",
              flash_dev, (unsigned int)addr + off, (unsigned int)sz, rc);
            free(data_buf);
            return -1;
        }
        if (move) {
            off++;
            if (off > 16) {
                off = 0;
            }
        }
        end_time = os_time_get();
        cnt++;
    } while (end_time - start_time < 2 * OS_TICKS_PER_SEC);

    free(data_buf);
    return cnt;
}

static int
flash_speed_test_cli(const struct shell_cmd *cmd, int argc, char **argv,
                     struct streamer *streamer)
{
    char *ep;
    int flash_dev;
    uint32_t addr;
    uint32_t sz;
    int move;
    int cnt;
    int i;

    if (argc < 4) {
        streamer_printf(streamer,
          "flash_speed <flash_id> <addr> <rd_sz>|range [move]\n");
        return 0;
    }

    flash_dev = strtoul(argv[1], &ep, 10);
    if (*ep != '\0') {
        streamer_printf(streamer, "Invalid flash_id: %s\n", argv[1]);
        return 0;
    }

    addr = strtoul(argv[2], &ep, 0);
    if (*ep != '\0') {
        streamer_printf(streamer, "Invalid address: %s\n", argv[2]);
        return 0;
    }

    if (!strcmp(argv[3], "range")) {
        i = 1;
    } else {
        i = 0;
        sz = strtoul(argv[3], &ep, 0);
        if (*ep != '\0') {
            streamer_printf(streamer, "Invalid read size: %s\n", argv[3]);
            return 0;
        }
    }
    if (argc > 4 && !strcmp(argv[4], "move")) {
        move = 1;
    } else {
        move = 0;
    }

    if (i == 0) {
        streamer_printf(streamer,
          "Speed test, hal_flash_read(%d, 0x%x%s, %d)\n",
          flash_dev, (unsigned int)addr, move?"..":"", (unsigned int)sz);
        cnt = flash_speed_test(flash_dev, addr, sz, move);
        streamer_printf(streamer, "%d (%d kB/s)\n", cnt >> 1, (cnt * sz) >> 11);
    } else {
        uint32_t sizes[] = {
            1, 2, 4, 8, 16, 24, 32, 48, 64, 96, 128, 192, 256
        };

        streamer_printf(streamer,
          "Speed test, hal_flash_read(%d, 0x%x%s, X)\n",
          flash_dev, (unsigned int)addr, move?"..":"");
        streamer_printf(streamer, " X  reads/s  kB/s\n");

        for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            cnt = flash_speed_test(flash_dev, addr, sizes[i], move);
            streamer_printf(streamer, "%3d %7d %5d\n", (int)sizes[i], cnt >> 1, (cnt * sizes[i]) >> 11);
            os_time_delay(OS_TICKS_PER_SEC / 8);
        }
    }
    return 0;
}

/*
 * Initialize the package. Only called from sysinit().
 */
void
flash_test_init(void)
{
    shell_cmd_register(&flash_cmd_struct);
    shell_cmd_register(&flash_speed_cli_struct);
}
