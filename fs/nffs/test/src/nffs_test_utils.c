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
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "hal/hal_flash.h"
#include "testutil/testutil.h"
#include "fs/fs.h"
#include "nffs/nffs.h"
#include "nffs_test.h"
#include "nffs_test_priv.h"
#include "nffs_priv.h"

#if 0
#ifdef ARCH_sim
const struct nffs_area_desc nffs_sim_area_descs[] = {
        { 0x00000000, 16 * 1024 },
        { 0x00004000, 16 * 1024 },
        { 0x00008000, 16 * 1024 },
        { 0x0000c000, 16 * 1024 },
        { 0x00010000, 64 * 1024 },
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0x00060000, 128 * 1024 },
        { 0x00080000, 128 * 1024 },
        { 0x000a0000, 128 * 1024 },
        { 0x000c0000, 128 * 1024 },
        { 0x000e0000, 128 * 1024 },
        { 0, 0 },
};
#endif
#endif

void
nffs_test_util_assert_ent_name(struct fs_dirent *dirent,
                               const char *expected_name)
{
    /* It should not be necessary to initialize this array, but the libgcc
     * version of strcmp triggers a "Conditional jump or move depends on
     * uninitialised value(s)" valgrind warning.
     */
    char name[NFFS_FILENAME_MAX_LEN + 1] = { 0 };
    uint8_t name_len;
    int rc;

    rc = fs_dirent_name(dirent, sizeof name, name, &name_len);
    TEST_ASSERT(rc == 0);
    if (rc == 0) {
        TEST_ASSERT(strcmp(name, expected_name) == 0);
    }
}

void
nffs_test_util_assert_file_len(struct fs_file *file, uint32_t expected)
{
    uint32_t len;
    int rc;

    rc = fs_filelen(file, &len);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(len == expected);
}

void
nffs_test_util_assert_cache_is_sane(const char *filename)
{
    struct nffs_cache_inode *cache_inode;
    struct nffs_cache_block *cache_block;
    struct fs_file *fs_file;
    struct nffs_file *file;
    uint32_t cache_start;
    uint32_t cache_end;
    uint32_t block_end;
    int rc;

    rc = fs_open(filename, FS_ACCESS_READ, &fs_file);
    TEST_ASSERT(rc == 0);

    file = (struct nffs_file *)fs_file;
    rc = nffs_cache_inode_ensure(&cache_inode, file->nf_inode_entry);
    TEST_ASSERT(rc == 0);

    nffs_cache_inode_range(cache_inode, &cache_start, &cache_end);

    if (TAILQ_EMPTY(&cache_inode->nci_block_list)) {
        TEST_ASSERT(cache_start == 0 && cache_end == 0);
    } else {
        block_end = 0;  /* Pacify gcc. */
        TAILQ_FOREACH(cache_block, &cache_inode->nci_block_list, ncb_link) {
            if (cache_block == TAILQ_FIRST(&cache_inode->nci_block_list)) {
                TEST_ASSERT(cache_block->ncb_file_offset == cache_start);
            } else {
                /* Ensure no gap between this block and its predecessor. */
                TEST_ASSERT(cache_block->ncb_file_offset == block_end);
            }

            block_end = cache_block->ncb_file_offset +
                        cache_block->ncb_block.nb_data_len;
            if (cache_block == TAILQ_LAST(&cache_inode->nci_block_list,
                                          nffs_cache_block_list)) {

                TEST_ASSERT(block_end == cache_end);
            }
        }
    }

    rc = fs_close(fs_file);
    TEST_ASSERT(rc == 0);
}

void
nffs_test_util_assert_contents(const char *filename, const char *contents,
                               int contents_len)
{
    struct fs_file *file;
    uint32_t bytes_read;
    void *buf;
    int rc;

    rc = fs_open(filename, FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    buf = malloc(contents_len + 1);
    TEST_ASSERT(buf != NULL);

    rc = fs_read(file, contents_len + 1, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == contents_len);
    TEST_ASSERT(memcmp(buf, contents, contents_len) == 0);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    free(buf);

    nffs_test_util_assert_cache_is_sane(filename);
}

int
nffs_test_util_block_count(const char *filename)
{
    struct nffs_hash_entry *entry;
    struct nffs_block block;
    struct nffs_file *file;
    struct fs_file *fs_file;
    int count;
    int rc;

    rc = fs_open(filename, FS_ACCESS_READ, &fs_file);
    TEST_ASSERT(rc == 0);

    file = (struct nffs_file *)fs_file;
    count = 0;
    entry = file->nf_inode_entry->nie_last_block_entry;
    while (entry != NULL) {
        count++;
        rc = nffs_block_from_hash_entry(&block, entry);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(block.nb_prev != entry);
        entry = block.nb_prev;
    }

    rc = fs_close(fs_file);
    TEST_ASSERT(rc == 0);

    return count;
}

void
nffs_test_util_assert_block_count(const char *filename, int expected_count)
{
    int actual_count;

    actual_count = nffs_test_util_block_count(filename);
    TEST_ASSERT(actual_count == expected_count);
}

void
nffs_test_util_assert_cache_range(const char *filename,
                                 uint32_t expected_cache_start,
                                 uint32_t expected_cache_end)
{
    struct nffs_cache_inode *cache_inode;
    struct nffs_file *file;
    struct fs_file *fs_file;
    uint32_t cache_start;
    uint32_t cache_end;
    int rc;

    rc = fs_open(filename, FS_ACCESS_READ, &fs_file);
    TEST_ASSERT(rc == 0);

    file = (struct nffs_file *)fs_file;
    rc = nffs_cache_inode_ensure(&cache_inode, file->nf_inode_entry);
    TEST_ASSERT(rc == 0);

    nffs_cache_inode_range(cache_inode, &cache_start, &cache_end);
    TEST_ASSERT(cache_start == expected_cache_start);
    TEST_ASSERT(cache_end == expected_cache_end);

    rc = fs_close(fs_file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_cache_is_sane(filename);
}

void
nffs_test_util_create_file_blocks(const char *filename,
                                 const struct nffs_test_block_desc *blocks,
                                 int num_blocks)
{
    struct fs_file *file;
    uint32_t total_len;
    uint32_t offset;
    char *buf;
    int num_writes;
    int rc;
    int i;

    rc = fs_open(filename, FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE, &file);
    TEST_ASSERT(rc == 0);

    total_len = 0;
    if (num_blocks <= 0) {
        num_writes = 1;
    } else {
        num_writes = num_blocks;
    }
    for (i = 0; i < num_writes; i++) {
        rc = fs_write(file, blocks[i].data, blocks[i].data_len);
        TEST_ASSERT(rc == 0);

        total_len += blocks[i].data_len;
    }

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    buf = malloc(total_len);
    TEST_ASSERT(buf != NULL);

    offset = 0;
    for (i = 0; i < num_writes; i++) {
        memcpy(buf + offset, blocks[i].data, blocks[i].data_len);
        offset += blocks[i].data_len;
    }
    TEST_ASSERT(offset == total_len);

    nffs_test_util_assert_contents(filename, buf, total_len);
    if (num_blocks > 0) {
        nffs_test_util_assert_block_count(filename, num_blocks);
    }

    free(buf);
}

void
nffs_test_util_create_file(const char *filename, const char *contents,
                           int contents_len)
{
    struct nffs_test_block_desc block;

    block.data = contents;
    block.data_len = contents_len;

    nffs_test_util_create_file_blocks(filename, &block, 0);
}

void
nffs_test_util_append_file(const char *filename, const char *contents,
                           int contents_len)
{
    struct fs_file *file;
    int rc;

    rc = fs_open(filename, FS_ACCESS_WRITE | FS_ACCESS_APPEND, &file);
    TEST_ASSERT(rc == 0);

    rc = fs_write(file, contents, contents_len);
    TEST_ASSERT(rc == 0);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);
}

void
nffs_test_copy_area(const struct nffs_area_desc *from,
                    const struct nffs_area_desc *to)
{
    void *buf;
    int rc;

    TEST_ASSERT(from->nad_length == to->nad_length);

    buf = malloc(from->nad_length);
    TEST_ASSERT(buf != NULL);

    rc = hal_flash_read(from->nad_flash_id, from->nad_offset, buf,
                        from->nad_length);
    TEST_ASSERT(rc == 0);

    rc = hal_flash_erase(from->nad_flash_id, to->nad_offset, to->nad_length);
    TEST_ASSERT(rc == 0);

    rc = hal_flash_write(to->nad_flash_id, to->nad_offset, buf, to->nad_length);
    TEST_ASSERT(rc == 0);

    free(buf);
}

void
nffs_test_util_create_subtree(const char *parent_path,
                             const struct nffs_test_file_desc *elem)
{
    char *path;
    int rc;
    int i;

    if (parent_path == NULL) {
        path = malloc(1);
        TEST_ASSERT(path != NULL);
        path[0] = '\0';
    } else {
        path = malloc(strlen(parent_path) + 1 + strlen(elem->filename) + 1);
        TEST_ASSERT(path != NULL);

        sprintf(path, "%s/%s", parent_path, elem->filename);
    }

    if (elem->is_dir) {
        if (parent_path != NULL) {
            rc = fs_mkdir(path);
            TEST_ASSERT(rc == 0);
        }

        if (elem->children != NULL) {
            for (i = 0; elem->children[i].filename != NULL; i++) {
                nffs_test_util_create_subtree(path, elem->children + i);
            }
        }
    } else {
        nffs_test_util_create_file(path, elem->contents, elem->contents_len);
    }

    free(path);
}

void
nffs_test_util_create_tree(const struct nffs_test_file_desc *root_dir)
{
    nffs_test_util_create_subtree(NULL, root_dir);
}

#define NFFS_TEST_TOUCHED_ARR_SZ     (16 * 64)
/*#define NFFS_TEST_TOUCHED_ARR_SZ     (16 * 1024)*/
struct nffs_hash_entry
    *nffs_test_touched_entries[NFFS_TEST_TOUCHED_ARR_SZ];
int nffs_test_num_touched_entries;

/*
 * Recursively descend directory structure
 */
void
nffs_test_assert_file(const struct nffs_test_file_desc *file,
                     struct nffs_inode_entry *inode_entry,
                     const char *path)
{
    const struct nffs_test_file_desc *child_file;
    struct nffs_inode inode;
    struct nffs_inode_entry *child_inode_entry;
    char *child_path;
    int child_filename_len;
    int path_len;
    int rc;

    /*
     * track of hash entries that have been examined
     */
    TEST_ASSERT(nffs_test_num_touched_entries < NFFS_TEST_TOUCHED_ARR_SZ);
    nffs_test_touched_entries[nffs_test_num_touched_entries] =
        &inode_entry->nie_hash_entry;
    nffs_test_num_touched_entries++;

    path_len = strlen(path);

    rc = nffs_inode_from_entry(&inode, inode_entry);
    TEST_ASSERT(rc == 0);

    /*
     * recursively examine each child of directory
     */
    if (nffs_hash_id_is_dir(inode_entry->nie_hash_entry.nhe_id)) {
        for (child_file = file->children;
             child_file != NULL && child_file->filename != NULL;
             child_file++) {

            /*
             * Construct full pathname for file
             * Not null terminated
             */
            child_filename_len = strlen(child_file->filename);
            child_path = malloc(path_len + 1 + child_filename_len + 1);
            TEST_ASSERT(child_path != NULL);
            memcpy(child_path, path, path_len);
            child_path[path_len] = '/';
            memcpy(child_path + path_len + 1, child_file->filename,
                   child_filename_len);
            child_path[path_len + 1 + child_filename_len] = '\0';

            /*
             * Verify child inode can be found using full pathname
             */
            rc = nffs_path_find_inode_entry(child_path, &child_inode_entry);
            if (rc != 0) {
                TEST_ASSERT(rc == 0);
            }

            nffs_test_assert_file(child_file, child_inode_entry, child_path);

            free(child_path);
        }
    } else {
        nffs_test_util_assert_contents(path, file->contents,
                                       file->contents_len);
    }
}

void
nffs_test_assert_branch_touched(struct nffs_inode_entry *inode_entry)
{
    struct nffs_inode_entry *child;
    int i;

    if (inode_entry == nffs_lost_found_dir) {
        return;
    }

    for (i = 0; i < nffs_test_num_touched_entries; i++) {
        if (nffs_test_touched_entries[i] == &inode_entry->nie_hash_entry) {
            break;
        }
    }
    TEST_ASSERT(i < nffs_test_num_touched_entries);
    nffs_test_touched_entries[i] = NULL;

    if (nffs_hash_id_is_dir(inode_entry->nie_hash_entry.nhe_id)) {
        SLIST_FOREACH(child, &inode_entry->nie_child_list, nie_sibling_next) {
            nffs_test_assert_branch_touched(child);
        }
    }
}

void
nffs_test_assert_child_inode_present(struct nffs_inode_entry *child)
{
    const struct nffs_inode_entry *inode_entry;
    const struct nffs_inode_entry *parent;
    struct nffs_inode inode;
    int rc;

    /*
     * Sucessfully read inode data from flash
     */
    rc = nffs_inode_from_entry(&inode, child);
    TEST_ASSERT(rc == 0);

    /*
     * Validate parent
     */
    parent = inode.ni_parent;
    TEST_ASSERT(parent != NULL);
    TEST_ASSERT(nffs_hash_id_is_dir(parent->nie_hash_entry.nhe_id));

    /*
     * Make sure inode is in parents child list
     */
    SLIST_FOREACH(inode_entry, &parent->nie_child_list, nie_sibling_next) {
        if (inode_entry == child) {
            return;
        }
    }

    TEST_ASSERT(0);
}

void
nffs_test_assert_block_present(struct nffs_hash_entry *block_entry)
{
    const struct nffs_inode_entry *inode_entry;
    struct nffs_hash_entry *cur;
    struct nffs_block block;
    int rc;

    /*
     * Successfully read block data from flash
     */
    rc = nffs_block_from_hash_entry(&block, block_entry);
    TEST_ASSERT(rc == 0);

    /*
     * Validate owning inode
     */
    inode_entry = block.nb_inode_entry;
    TEST_ASSERT(inode_entry != NULL);
    TEST_ASSERT(nffs_hash_id_is_file(inode_entry->nie_hash_entry.nhe_id));

    /*
     * Validate that block is in owning inode's block chain
     */
    cur = inode_entry->nie_last_block_entry;
    while (cur != NULL) {
        if (cur == block_entry) {
            return;
        }

        rc = nffs_block_from_hash_entry(&block, cur);
        TEST_ASSERT(rc == 0);
        cur = block.nb_prev;
    }

    TEST_ASSERT(0);
}

/*
 * Recursively verify that the children of each directory are sorted
 * on the directory children linked list by filename length
 */
void
nffs_test_assert_children_sorted(struct nffs_inode_entry *inode_entry)
{
    struct nffs_inode_entry *child_entry;
    struct nffs_inode_entry *prev_entry;
    struct nffs_inode child_inode;
    struct nffs_inode prev_inode;
    int cmp;
    int rc;

    prev_entry = NULL;
    SLIST_FOREACH(child_entry, &inode_entry->nie_child_list,
                  nie_sibling_next) {
        rc = nffs_inode_from_entry(&child_inode, child_entry);
        TEST_ASSERT(rc == 0);

        if (prev_entry != NULL) {
            rc = nffs_inode_from_entry(&prev_inode, prev_entry);
            TEST_ASSERT(rc == 0);

            rc = nffs_inode_filename_cmp_flash(&prev_inode, &child_inode,
                                               &cmp);
            TEST_ASSERT(rc == 0);
            TEST_ASSERT(cmp < 0);
        }

        if (nffs_hash_id_is_dir(child_entry->nie_hash_entry.nhe_id)) {
            nffs_test_assert_children_sorted(child_entry);
        }

        prev_entry = child_entry;
    }
}

void
nffs_test_assert_system_once(const struct nffs_test_file_desc *root_dir)
{
    struct nffs_inode_entry *inode_entry;
    struct nffs_hash_entry *entry;
    struct nffs_hash_entry *next;
    int i;

    nffs_test_num_touched_entries = 0;
    nffs_test_assert_file(root_dir, nffs_root_dir, "");
    nffs_test_assert_branch_touched(nffs_root_dir);

    /* Ensure no orphaned inodes or blocks. */
    NFFS_HASH_FOREACH(entry, i, next) {
        TEST_ASSERT(entry->nhe_flash_loc != NFFS_FLASH_LOC_NONE);
        if (nffs_hash_id_is_inode(entry->nhe_id)) {
            inode_entry = (void *)entry;
            TEST_ASSERT(inode_entry->nie_refcnt == 1);
            if (entry->nhe_id == NFFS_ID_ROOT_DIR) {
                TEST_ASSERT(inode_entry == nffs_root_dir);
            } else {
                nffs_test_assert_child_inode_present(inode_entry);
            }
        } else {
            nffs_test_assert_block_present(entry);
        }
    }

    /* Ensure proper sorting. */
    nffs_test_assert_children_sorted(nffs_root_dir);
}

void
nffs_test_assert_system(const struct nffs_test_file_desc *root_dir,
                        const struct nffs_area_desc *area_descs)
{
    int rc;

    /* Ensure files are as specified, and that there are no other files or
     * orphaned inodes / blocks.
     */
    nffs_test_assert_system_once(root_dir);

    /* Force a garbage collection cycle. */
    rc = nffs_gc(NULL);
    TEST_ASSERT(rc == 0);

    /* Ensure file system is still as expected. */
    nffs_test_assert_system_once(root_dir);

    /* Clear cached data and restore from flash (i.e, simulate a reboot). */
    rc = nffs_misc_reset();
    TEST_ASSERT(rc == 0);
    rc = nffs_detect(area_descs);
    TEST_ASSERT(rc == 0);

    /* Ensure file system is still as expected. */
    nffs_test_assert_system_once(root_dir);
}

void
nffs_test_assert_area_seqs(int seq1, int count1, int seq2, int count2)
{
    struct nffs_disk_area disk_area;
    int cur1;
    int cur2;
    int rc;
    int i;

    cur1 = 0;
    cur2 = 0;

    for (i = 0; i < nffs_num_areas; i++) {
        rc = nffs_flash_read(i, 0, &disk_area, sizeof disk_area);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(nffs_area_magic_is_set(&disk_area));
        TEST_ASSERT(disk_area.nda_gc_seq == nffs_areas[i].na_gc_seq);
        if (i == nffs_scratch_area_idx) {
            TEST_ASSERT(disk_area.nda_id == NFFS_AREA_ID_NONE);
        }

        if (nffs_areas[i].na_gc_seq == seq1) {
            cur1++;
        } else if (nffs_areas[i].na_gc_seq == seq2) {
            cur2++;
        } else {
            TEST_ASSERT(0);
        }
    }

    TEST_ASSERT(cur1 == count1 && cur2 == count2);
}

#if 0
void
nffs_test_mkdir(void)
{
    struct fs_file *file;
    int rc;


    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/a/b/c/d");
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_mkdir("asdf");
    TEST_ASSERT(rc == FS_EINVAL);

    rc = fs_mkdir("/a");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/a/b");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/a/b/c");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/a/b/c/d");
    TEST_ASSERT(rc == 0);

    rc = fs_open("/a/b/c/d/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "a",
                .is_dir = 1,
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = "b",
                    .is_dir = 1,
                    .children = (struct nffs_test_file_desc[]) { {
                        .filename = "c",
                        .is_dir = 1,
                        .children = (struct nffs_test_file_desc[]) { {
                            .filename = "d",
                            .is_dir = 1,
                            .children = (struct nffs_test_file_desc[]) { {
                                .filename = "myfile.txt",
                                .contents = NULL,
                                .contents_len = 0,
                            }, {
                                .filename = NULL,
                            } },
                        }, {
                            .filename = NULL,
                        } },
                    }, {
                        .filename = NULL,
                    } },
                }, {
                    .filename = NULL,
                } },
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
#endif
