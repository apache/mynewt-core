/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * XXX
 * This is a hack of a tool which prints the structure of an nffs file system.
 * It needs to be rewritten properly.
 */

#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include "../src/nffs_priv.h"
#include <os/queue.h>
#include <fs/fs.h>
#include <nffs/nffs.h>
#include <hal/hal_flash.h>
#include <util/flash_map.h>
#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

static const char *copy_in_dir;
static const char *progname;

#define MAX_AREAS	16
static struct nffs_area_desc area_descs[MAX_AREAS];

static void usage(int rc);

static void
copyfs(FILE *fp)
{
    uint32_t dst_addr;
    int rc;
    int c;

    dst_addr = area_descs[0].nad_offset;
    while (1) {
        c = getc(fp);
        if (c == EOF) {
            return;
        }

        rc = hal_flash_write(area_descs[0].nad_flash_id, dst_addr, &c, 1);
        assert(rc == 0);

        dst_addr++;
    }
}

static void
print_inode_entry(struct nffs_inode_entry *inode_entry, int indent)
{
    struct nffs_inode inode;
    char name[NFFS_FILENAME_MAX_LEN + 1];
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    rc = nffs_inode_from_entry(&inode, inode_entry);
    assert(rc == 0);

    nffs_flash_loc_expand(inode_entry->nie_hash_entry.nhe_flash_loc,
                         &area_idx, &area_offset);

    rc = nffs_flash_read(area_idx,
                         area_offset + sizeof (struct nffs_disk_inode),
                         name, inode.ni_filename_len);
    assert(rc == 0);

    name[inode.ni_filename_len] = '\0';

    printf("%*s%s\n", indent, "", name[0] == '\0' ? "/" : name);
}

static void
process_inode_entry(struct nffs_inode_entry *inode_entry, int indent)
{
    struct nffs_inode_entry *child;

    print_inode_entry(inode_entry, indent);

    if (nffs_hash_id_is_dir(inode_entry->nie_hash_entry.nhe_id)) {
        SLIST_FOREACH(child, &inode_entry->nie_child_list, nie_sibling_next) {
            process_inode_entry(child, indent + 2);
        }
    }
}

static void
printfs(void)
{
    printf("\n\nNFFS contents:\n");
    process_inode_entry(nffs_root_dir, 0);
}

static int
copy_in_file(char *src, char *dst)
{
    struct fs_file *nf;
    FILE *fp;
    int rc;
    char data[2048];
    int ret = 0;

    rc = fs_open(dst, FS_ACCESS_WRITE, &nf);
    assert(rc == 0);

    fp = fopen(src, "r");
    if (!fp) {
        perror("fopen()");
        assert(0);
    }
    while ((rc = fread(data, 1, sizeof(data), fp))) {
        rc = fs_write(nf, data, rc);
        if (rc) {
            ret = rc;
            break;
        }
    }
    rc = fs_close(nf);
    assert(rc == 0);
    fclose(fp);
    return ret;
}

void
copy_in_directory(const char *src, const char *dst)
{
    DIR *dr;
    struct dirent *entry;
    char src_name[1024];
    char dst_name[1024];
    int rc;

    dr = opendir(src);
    if (!dr) {
        perror("opendir");
        usage(1);
    }
    while ((entry = readdir(dr))) {
        snprintf(src_name, sizeof(src_name), "%s/%s", src, entry->d_name);
        snprintf(dst_name, sizeof(dst_name), "%s/%s", dst, entry->d_name);
        if (entry->d_type == DT_DIR &&
          strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            rc = fs_mkdir(dst_name);
            copy_in_directory(src_name, dst_name);
            assert(rc == 0);
        } else if (entry->d_type == DT_REG) {
            printf("Copying %s\n", dst_name);
            rc = copy_in_file(src_name, dst_name);
            if (rc) {
                printf("  error code %d ", rc);
                switch (rc) {
                case FS_ENOMEM:
                    printf("out of memory\n");
                    break;
                case FS_EFULL:
                    printf("disk is full\n");
                    break;
                default:
                    printf("\n");
                }
                break;
            }
        } else {
            printf("Skipping %s\n", src_name);
        }
    }
    closedir(dr);
}

static void
usage(int rc)
{
    printf("%s [-c]|[-d dir][-f flash_file]\n", progname);
    printf("  Tool for operating on simulator flash image file\n");
    printf("   -c: ...\n");
    printf("   -d: use dir as root for NFFS portion and create flash image\n");
    printf("   -f: flash_file is the name of the flash image file\n");
    exit(rc);
}

int
main(int argc, char **argv)
{
    FILE *fp;
    int rc;
    int ch;
    int cnt;

    progname = argv[0];

    while ((ch = getopt(argc, argv, "c:d:f:")) != -1) {
        switch (ch) {
        case 'c':
            fp = fopen(optarg, "rb");
            assert(fp != NULL);
            copyfs(fp);
            fclose(fp);
            break;
        case 'd':
            copy_in_dir = optarg;
            break;
        case 'f':
            native_flash_file = optarg;
            break;
        case '?':
        default:
            usage(0);
        }
    }

    os_init();
    rc = flash_area_to_nffs_desc(FLASH_AREA_NFFS, &cnt, area_descs);
    assert(rc == 0);

    rc = hal_flash_init();
    assert(rc == 0);

    rc = nffs_init();
    assert(rc == 0);

    if (copy_in_dir) {
        /*
         * Build filesystem from contents of directory
         */
        rc = nffs_format(area_descs);
        assert(rc == 0);
        copy_in_directory(copy_in_dir, "");
    } else {
        rc = nffs_detect(area_descs);
        if (rc) {
            printf("nffs_detect() failed\n");
            exit(0);
        }
    }
    printfs();

    return 0;
}
