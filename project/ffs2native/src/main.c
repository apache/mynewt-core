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
#include "../src/nffs_priv.h"
#include "os/queue.h"
#include "nffs/nffs.h"
#include "hal/hal_flash.h"

static const struct nffs_area_desc area_descs[] = {
    { 0x00020000, 128 * 1024 },
    { 0x00040000, 128 * 1024 },
    { 0, 0 },
};

static void
copyfs(FILE *fp)
{
    uint32_t dst_addr;
    int rc;
    int c;

    dst_addr = area_descs[0].fad_offset;
    while (1) {
        c = getc(fp);
        if (c == EOF) {
            return;
        }

        rc = flash_write(dst_addr, &c, 1);
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

    nffs_flash_loc_expand(inode_entry->fie_hash_entry.fhe_flash_loc,
                         &area_idx, &area_offset);

    rc = nffs_flash_read(area_idx,
                         area_offset + sizeof (struct nffs_disk_inode),
                         name, inode.fi_filename_len);
    assert(rc == 0);

    name[inode.fi_filename_len] = '\0';

    printf("%*s%s\n", indent, "", name);
}

static void
process_inode_entry(struct nffs_inode_entry *inode_entry, int indent)
{
    struct nffs_inode_entry *child;

    print_inode_entry(inode_entry, indent);

    if (nffs_hash_id_is_dir(inode_entry->fie_hash_entry.fhe_id)) {
        SLIST_FOREACH(child, &inode_entry->fie_child_list, fie_sibling_next) {
            process_inode_entry(child, indent + 2);
        }
    }
}

static void
printfs(void)
{
    process_inode_entry(nffs_root_dir, 0);
}

int
main(int argc, char **argv)
{
    FILE *fp;
    int rc;

    assert(argc >= 2);

    fp = fopen(argv[1], "rb");
    assert(fp != NULL);

    copyfs(fp);

    rc = nffs_init();
    assert(rc == 0);

    rc = nffs_detect(area_descs);
    assert(rc == 0);

    printfs();

    return 0;
}
