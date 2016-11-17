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

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

#include <fatfs/ff.h>

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
    FATFS_DIR dir;
    FILINFO fileinfo;
    FIL file;
    UINT bytes_read;
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

    //TODO
    //log_register("fatfs-log", &nffs_log, &log_console_handler, NULL, LOG_SYSLEVEL);

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

    res = f_opendir(&dir, "0:/");
    if (res) {
        printf("f_opendir() failed %d\n", res);
        exit(1);
    }

    printf("\nListing 0:/\n");

    while (1) {
        res = f_readdir(&dir, &fileinfo);
        if (res) {
            printf("f_readdir() failed %d\n", res);
            exit(1);
        }

        /* last entry in a dir always returned as NULL */
        if (!fileinfo.fname[0]) {
            break;
        }

        printf("%s\t\t%8lu bytes\n", fileinfo.fname, fileinfo.fsize);
    }

    /* If a README.txt exists, print contents */
    res = f_open(&file, "0:/README.txt", FA_READ);
    if (res == FR_OK) {
        printf("\nREADME.txt found, showing contents:\n\n");
        printf("------------------------------------------------\n");

        while (1) {
            res = f_read(&file, buf, 32, &bytes_read);
            printf("%s\n", buf);
            if (bytes_read != 32) {
                break;
            }
        }

        printf("------------------------------------------------\n");
        f_close(&file);
    }

    f_closedir(&dir);
    return 0;
}
