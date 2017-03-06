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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sysinit/sysinit.h>
#include <fs/fs.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

#include <fatfs/ff.h>
#include <fs/fs_if.h>

extern const struct fs_ops *fs_root_ops;
static const char *progname;

static void
usage(int rc)
{
    printf("%s [-h][-f image_file]\n", progname);
    printf("  Tool for operating on simulator flash image file\n");
    printf("   -f: flash_file is the name of the flash image file\n");
    exit(rc);
}

int
main(int argc, char **argv)
{
    int ch;
    FATFS fs;
    FATFS *p_fs;
    DWORD fre_clust, fre_sect, tot_sect;
    FRESULT res;
    struct fs_dir *dir;
    struct fs_dirent *dirent;
    struct fs_file *file;
    char out_name[80];
    uint8_t u8_len;
    uint32_t u32_len;
    int rc;
    uint8_t buf[32];
    static const BYTE ft[] = {0, 12, 16, 32};

    progname = argv[0];

    while ((ch = getopt(argc, argv, "f:v")) != -1) {
        switch (ch) {
        case 'f':
            native_flash_file = optarg;
            break;
        case '?':
        case 'h':
        default:
            usage(0);
        }
    }

    sysinit();

    /* TODO:
     * log_register("fatfs-log", &nffs_log, &log_console_handler, NULL, LOG_SYSLEVEL);
     */

    f_mount(&fs, "0:", 0);

    /* Get volume information and free clusters of drive 1 */
    res = f_getfree("0:", &fre_clust, &p_fs);
    if (res) {
        printf("f_getfree() failed %d\n", res);
        exit(1);
    }

    printf("\nFAT type = FAT%u\n"
           "Number of FATs = %u\n"
           "Root DIR entries = %u\n"
           "Sectors/FAT = %lu\n"
           "Volume start = %lu\n"
           "FAT start = %lu\n"
           "DIR start = %lu\n"
           "Data start = %lu\n\n",
        ft[p_fs->fs_type & 3], p_fs->n_fats, p_fs->n_rootdir, p_fs->fsize,
        p_fs->volbase, p_fs->fatbase, p_fs->dirbase, p_fs->database);

    /* Print the free space (assuming 512 bytes/sector) */

    tot_sect = (p_fs->n_fatent - 2) * p_fs->csize;
    fre_sect = fre_clust * p_fs->csize;

    printf("%8lu KiB total drive space.\n"
           "%8lu KiB available.\n",
           tot_sect / 2, fre_sect / 2);

    /* List contents of root directory */

    rc = fs_root_ops->f_opendir("0:/", &dir);
    if (rc != FS_EOK) {
        printf("f_opendir() failed %d\n", rc);
        exit(1);
    }

    printf("\nListing 0:/\n");

    while (1) {
        rc = fs_root_ops->f_readdir(dir, &dirent);
        if (rc == FS_ENOENT) {
            break;
        } else if (rc != FS_EOK) {
            printf("f_readdir() failed %d\n", rc);
            exit(1);
        }

        rc = fs_root_ops->f_dirent_name(dirent, sizeof(out_name), out_name, &u8_len);
        printf("%s\n", out_name);
    }

    fs_root_ops->f_closedir(dir);

    /* If a README.txt exists, print contents */
    rc = fs_root_ops->f_open("0:/README.txt", FS_ACCESS_READ, &file);
    if (rc == FS_EOK) {
        printf("\nREADME.txt found, showing contents:\n\n");
        printf("------------------------------------------------\n");

        while (1) {
            rc = fs_root_ops->f_read(file, sizeof(buf), buf, &u32_len);
            printf("%s\n", buf);
            if (u32_len != 32) {
                break;
            }
        }

        printf("------------------------------------------------\n");
        fs_root_ops->f_close(file);
    }

    return 0;
}
