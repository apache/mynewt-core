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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "hal/hal_flash.h"
#include "testutil/testutil.h"
#include "fs/fs.h"
#include "nffs/nffs.h"
#include "nffs/nffs_test.h"
#include "nffs_test_priv.h"
#include "nffs_priv.h"

int flash_native_memset(uint32_t offset, uint8_t c, uint32_t len);

static const struct nffs_area_desc nffs_area_descs[] = {
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

static void
nffs_test_util_assert_ent_name(struct fs_dirent *dirent,
                               const char *expected_name)
{
    char name[NFFS_FILENAME_MAX_LEN + 1];
    uint8_t name_len;
    int rc;

    rc = fs_dirent_name(dirent, sizeof name, name, &name_len);
    TEST_ASSERT(rc == 0);
    if (rc == 0) {
        TEST_ASSERT(strcmp(name, expected_name) == 0);
    }
}

static void
nffs_test_util_assert_file_len(struct fs_file *file, uint32_t expected)
{
    uint32_t len;
    int rc;

    rc = fs_filelen(file, &len);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(len == expected);
}

static void
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

static void
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

static int
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

static void
nffs_test_util_assert_block_count(const char *filename, int expected_count)
{
    int actual_count;

    actual_count = nffs_test_util_block_count(filename);
    TEST_ASSERT(actual_count == expected_count);
}

static void
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

static void
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

static void
nffs_test_util_create_file(const char *filename, const char *contents,
                           int contents_len)
{
    struct nffs_test_block_desc block;

    block.data = contents;
    block.data_len = contents_len;

    nffs_test_util_create_file_blocks(filename, &block, 0);
}

static void
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

static void
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

static void
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

static void
nffs_test_util_create_tree(const struct nffs_test_file_desc *root_dir)
{
    nffs_test_util_create_subtree(NULL, root_dir);
}

#define NFFS_TEST_TOUCHED_ARR_SZ     (16 * 1024)
static struct nffs_hash_entry
    *nffs_test_touched_entries[NFFS_TEST_TOUCHED_ARR_SZ];
static int nffs_test_num_touched_entries;

/*
 * Recursively descend directory structure
 */
static void
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

static void
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

static void
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

static void
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
static void
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

static void
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

static void
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

static void
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

static void
nffs_test_mkdir(void)
{
    struct fs_file *file;
    int rc;


    rc = nffs_format(nffs_area_descs);
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

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_unlink)
{
    struct fs_file *file0;
    struct fs_file *file1;
    struct fs_file *file2;
    struct nffs_file *nfs_file;
    uint8_t buf[64];
    uint32_t bytes_read;
    int initial_num_blocks;
    int initial_num_inodes;
    int rc;

    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT_FATAL(rc == 0);

    initial_num_blocks = nffs_block_entry_pool.mp_num_free;
    initial_num_inodes = nffs_inode_entry_pool.mp_num_free;

    nffs_test_util_create_file("/file0.txt", "0", 1);

    rc = fs_open("/file0.txt", FS_ACCESS_READ | FS_ACCESS_WRITE, &file0);
    TEST_ASSERT(rc == 0);
    nfs_file = (struct nffs_file *)file0;
    TEST_ASSERT(nfs_file->nf_inode_entry->nie_refcnt == 2);

    rc = fs_unlink("/file0.txt");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(nfs_file->nf_inode_entry->nie_refcnt == 1);

    rc = fs_open("/file0.txt", FS_ACCESS_READ, &file2);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_write(file0, "00", 2);
    TEST_ASSERT(rc == 0);

    rc = fs_seek(file0, 0);
    TEST_ASSERT(rc == 0);

    rc = fs_read(file0, sizeof buf, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 2);
    TEST_ASSERT(memcmp(buf, "00", 2) == 0);

    rc = fs_close(file0);
    TEST_ASSERT(rc == 0);

    rc = fs_open("/file0.txt", FS_ACCESS_READ, &file0);
    TEST_ASSERT(rc == FS_ENOENT);

    /* Ensure the file was fully removed from RAM. */
    TEST_ASSERT(nffs_inode_entry_pool.mp_num_free == initial_num_inodes);
    TEST_ASSERT(nffs_block_entry_pool.mp_num_free == initial_num_blocks);

    /*** Nested unlink. */
    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);
    nffs_test_util_create_file("/mydir/file1.txt", "1", 2);

    rc = fs_open("/mydir/file1.txt", FS_ACCESS_READ | FS_ACCESS_WRITE, &file1);
    TEST_ASSERT(rc == 0);
    nfs_file = (struct nffs_file *)file1;
    TEST_ASSERT(nfs_file->nf_inode_entry->nie_refcnt == 2);

    rc = fs_unlink("/mydir");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(nfs_file->nf_inode_entry->nie_refcnt == 1);

    rc = fs_open("/mydir/file1.txt", FS_ACCESS_READ, &file2);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_write(file1, "11", 2);
    TEST_ASSERT(rc == 0);

    rc = fs_seek(file1, 0);
    TEST_ASSERT(rc == 0);

    rc = fs_read(file1, sizeof buf, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 2);
    TEST_ASSERT(memcmp(buf, "11", 2) == 0);

    rc = fs_close(file1);
    TEST_ASSERT(rc == 0);

    rc = fs_open("/mydir/file1.txt", FS_ACCESS_READ, &file1);
    TEST_ASSERT(rc == FS_ENOENT);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);

    /* Ensure the files and directories were fully removed from RAM. */
    TEST_ASSERT(nffs_inode_entry_pool.mp_num_free == initial_num_inodes);
    TEST_ASSERT(nffs_block_entry_pool.mp_num_free == initial_num_blocks);
}

TEST_CASE(nffs_test_rename)
{
    struct fs_file *file;
    const char contents[] = "contents";
    int rc;


    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_rename("/nonexistent.txt", "/newname.txt");
    TEST_ASSERT(rc == FS_ENOENT);

    /*** Rename file. */
    nffs_test_util_create_file("/myfile.txt", contents, sizeof contents);

    rc = fs_rename("/myfile.txt", "badname");
    TEST_ASSERT(rc == FS_EINVAL);

    rc = fs_rename("/myfile.txt", "/myfile2.txt");
    TEST_ASSERT(rc == 0);

    rc = fs_open("/myfile.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_ENOENT);

    nffs_test_util_assert_contents("/myfile2.txt", contents, sizeof contents);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/mydir/leafdir");
    TEST_ASSERT(rc == 0);

    rc = fs_rename("/myfile2.txt", "/mydir/myfile2.txt");
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/mydir/myfile2.txt", contents,
                                  sizeof contents);

    /*** Rename directory. */
    rc = fs_rename("/mydir", "badname");
    TEST_ASSERT(rc == FS_EINVAL);

    /* Don't allow a directory to be moved into a descendent directory. */
    rc = fs_rename("/mydir", "/mydir/leafdir/a");
    TEST_ASSERT(rc == FS_EINVAL);

    rc = fs_rename("/mydir", "/mydir2");
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/mydir2/myfile2.txt", contents,
                                  sizeof contents);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "mydir2",
                .is_dir = 1,
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = "leafdir",
                    .is_dir = 1,
                }, {
                    .filename = "myfile2.txt",
                    .contents = "contents",
                    .contents_len = 9,
                }, {
                    .filename = NULL,
                } },
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_truncate)
{
    struct fs_file *file;
    int rc;


    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 8);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abcdefgh", 8);

    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "1234", 4);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 4);
    TEST_ASSERT(fs_getpos(file) == 4);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "1234", 4);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "1234",
                .contents_len = 4,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_append)
{
    struct fs_file *file;
    uint32_t len;
    char c;
    int rc;
    int i;

    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE | FS_ACCESS_APPEND, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 8);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abcdefgh", 8);

    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE | FS_ACCESS_APPEND, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 8);

    /* File position should always be at the end of a file after an append.
     * Seek to the middle prior to writing to test this.
     */
    rc = fs_seek(file, 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 2);

    rc = fs_write(file, "ijklmnop", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 16);
    rc = fs_write(file, "qrstuvwx", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 24);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt",
                                  "abcdefghijklmnopqrstuvwx", 24);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT_FATAL(rc == 0);
    rc = fs_open("/mydir/gaga.txt", FS_ACCESS_WRITE | FS_ACCESS_APPEND, &file);
    TEST_ASSERT_FATAL(rc == 0);

    /*** Repeated appends to a large file. */
    for (i = 0; i < 1000; i++) {
        rc = fs_filelen(file, &len);
        TEST_ASSERT_FATAL(rc == 0);
        TEST_ASSERT(len == i);

        c = '0' + i % 10;
        rc = fs_write(file, &c, 1);
        TEST_ASSERT_FATAL(rc == 0);
    }

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/mydir/gaga.txt",
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789",
        1000);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdefghijklmnopqrstuvwx",
                .contents_len = 24,
            }, {
                .filename = "mydir",
                .is_dir = 1,
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = "gaga.txt",
                    .contents =
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    ,
                    .contents_len = 1000,
                }, {
                    .filename = NULL,
                } },
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_read)
{
    struct fs_file *file;
    uint8_t buf[16];
    uint32_t bytes_read;
    int rc;

    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/myfile.txt", "1234567890", 10);

    rc = fs_open("/myfile.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 10);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_read(file, 4, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 4);
    TEST_ASSERT(memcmp(buf, "1234", 4) == 0);
    TEST_ASSERT(fs_getpos(file) == 4);

    rc = fs_read(file, sizeof buf - 4, buf + 4, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 6);
    TEST_ASSERT(memcmp(buf, "1234567890", 10) == 0);
    TEST_ASSERT(fs_getpos(file) == 10);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(nffs_test_open)
{
    struct fs_file *file;
    struct fs_dir *dir;
    int rc;

    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Fail to open an invalid path (not rooted). */
    rc = fs_open("file", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_EINVAL);

    /*** Fail to open a directory (root directory). */
    rc = fs_open("/", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_EINVAL);

    /*** Fail to open a nonexistent file for reading. */
    rc = fs_open("/1234", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_ENOENT);

    /*** Fail to open a child of a nonexistent directory. */
    rc = fs_open("/dir/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == FS_ENOENT);
    rc = fs_opendir("/dir", &dir);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_mkdir("/dir");
    TEST_ASSERT(rc == 0);

    /*** Fail to open a directory. */
    rc = fs_open("/dir", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_EINVAL);

    /*** Successfully open an existing file for reading. */
    nffs_test_util_create_file("/dir/file.txt", "1234567890", 10);
    rc = fs_open("/dir/file.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    /*** Successfully open an nonexistent file for writing. */
    rc = fs_open("/dir/file2.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    /*** Ensure the file can be reopened. */
    rc = fs_open("/dir/file.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(nffs_test_overwrite_one)
{
    struct fs_file *file;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_append_file("/myfile.txt", "abcdefgh", 8);

    /*** Overwrite within one block (middle). */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 3);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 3);

    rc = fs_write(file, "12", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 5);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abc12fgh", 8);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite within one block (start). */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "xy", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 2);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "xyc12fgh", 8);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite within one block (end). */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "<>", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 8);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "xyc12f<>", 8);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite one block middle, extend. */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 4);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 4);

    rc = fs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 12);
    TEST_ASSERT(fs_getpos(file) == 12);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "xyc1abcdefgh", 12);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite one block start, extend. */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 12);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "abcdefghijklmnop", 16);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 16);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abcdefghijklmnop", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdefghijklmnop",
                .contents_len = 16,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_overwrite_two)
{
    struct nffs_test_block_desc *blocks = (struct nffs_test_block_desc[]) { {
        .data = "abcdefgh",
        .data_len = 8,
    }, {
        .data = "ijklmnop",
        .data_len = 8,
    } };

    struct fs_file *file;
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite two blocks (middle). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 7);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 7);

    rc = fs_write(file, "123", 3);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 10);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "abcdefg123klmnop", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks (start). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "ABCDEFGHIJ", 10);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 10);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "ABCDEFGHIJklmnop", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks (end). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890", 10);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 16);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks middle, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890!@#$", 14);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 20);
    TEST_ASSERT(fs_getpos(file) == 20);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890!@#$", 20);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks start, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 20);
    TEST_ASSERT(fs_getpos(file) == 20);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "1234567890!@#$%^&*()", 20);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "1234567890!@#$%^&*()",
                .contents_len = 20,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_overwrite_three)
{
    struct nffs_test_block_desc *blocks = (struct nffs_test_block_desc[]) { {
        .data = "abcdefgh",
        .data_len = 8,
    }, {
        .data = "ijklmnop",
        .data_len = 8,
    }, {
        .data = "qrstuvwx",
        .data_len = 8,
    } };

    struct fs_file *file;
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite three blocks (middle). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890!@", 12);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 18);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@stuvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks (start). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 20);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "1234567890!@#$%^&*()uvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks (end). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890!@#$%^&*", 18);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 24);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks middle, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 26);
    TEST_ASSERT(fs_getpos(file) == 26);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*()", 26);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks start, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "1234567890!@#$%^&*()abcdefghij", 30);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 30);
    TEST_ASSERT(fs_getpos(file) == 30);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "1234567890!@#$%^&*()abcdefghij", 30);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "1234567890!@#$%^&*()abcdefghij",
                .contents_len = 30,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_overwrite_many)
{
    struct nffs_test_block_desc *blocks = (struct nffs_test_block_desc[]) { {
        .data = "abcdefgh",
        .data_len = 8,
    }, {
        .data = "ijklmnop",
        .data_len = 8,
    }, {
        .data = "qrstuvwx",
        .data_len = 8,
    } };

    struct fs_file *file;
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite middle of first block. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 3);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 3);

    rc = fs_write(file, "12", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 5);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abc12fghijklmnopqrstuvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite end of first block, start of second. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234", 4);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 10);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234klmnopqrstuvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdef1234klmnopqrstuvwx",
                .contents_len = 24,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_long_filename)
{
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/12345678901234567890.txt", "contents", 8);

    rc = fs_mkdir("/longdir12345678901234567890");
    TEST_ASSERT(rc == 0);

    rc = fs_rename("/12345678901234567890.txt",
                    "/longdir12345678901234567890/12345678901234567890.txt");
    TEST_ASSERT(rc == 0);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "longdir12345678901234567890",
                .is_dir = 1,
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = "/12345678901234567890.txt",
                    .contents = "contents",
                    .contents_len = 8,
                }, {
                    .filename = NULL,
                } },
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_large_write)
{
    static char data[NFFS_BLOCK_MAX_DATA_SZ_MAX * 5];
    int rc;
    int i;

    static const struct nffs_area_desc area_descs_two[] = {
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0, 0 },
    };



    /*** Setup. */
    rc = nffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof data; i++) {
        data[i] = i;
    }

    nffs_test_util_create_file("/myfile.txt", data, sizeof data);

    /* Ensure large write was split across the appropriate number of data
     * blocks.
     */
    TEST_ASSERT(nffs_test_util_block_count("/myfile.txt") ==
           sizeof data / NFFS_BLOCK_MAX_DATA_SZ_MAX);

    /* Garbage collect and then ensure the large file is still properly divided
     * according to max data block size.
     */
    nffs_gc(NULL);
    TEST_ASSERT(nffs_test_util_block_count("/myfile.txt") ==
           sizeof data / NFFS_BLOCK_MAX_DATA_SZ_MAX);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = data,
                .contents_len = sizeof data,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, area_descs_two);
}

TEST_CASE(nffs_test_many_children)
{
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/zasdf", NULL, 0);
    nffs_test_util_create_file("/FfD", NULL, 0);
    nffs_test_util_create_file("/4Zvv", NULL, 0);
    nffs_test_util_create_file("/*(*2fs", NULL, 0);
    nffs_test_util_create_file("/pzzd", NULL, 0);
    nffs_test_util_create_file("/zasdf0", NULL, 0);
    nffs_test_util_create_file("/23132.bin", NULL, 0);
    nffs_test_util_create_file("/asldkfjaldskfadsfsdf.txt", NULL, 0);
    nffs_test_util_create_file("/sdgaf", NULL, 0);
    nffs_test_util_create_file("/939302**", NULL, 0);
    rc = fs_mkdir("/dir");
    nffs_test_util_create_file("/dir/itw82", NULL, 0);
    nffs_test_util_create_file("/dir/124", NULL, 0);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) {
                { "zasdf" },
                { "FfD" },
                { "4Zvv" },
                { "*(*2fs" },
                { "pzzd" },
                { "zasdf0" },
                { "23132.bin" },
                { "asldkfjaldskfadsfsdf.txt" },
                { "sdgaf" },
                { "939302**" },
                {
                    .filename = "dir",
                    .is_dir = 1,
                    .children = (struct nffs_test_file_desc[]) {
                        { "itw82" },
                        { "124" },
                        { NULL },
                    },
                },
                { NULL },
            }
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_gc)
{
    int rc;

    static const struct nffs_area_desc area_descs_two[] = {
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0, 0 },
    };

    struct nffs_test_block_desc blocks[8] = { {
        .data = "1",
        .data_len = 1,
    }, {
        .data = "2",
        .data_len = 1,
    }, {
        .data = "3",
        .data_len = 1,
    }, {
        .data = "4",
        .data_len = 1,
    }, {
        .data = "5",
        .data_len = 1,
    }, {
        .data = "6",
        .data_len = 1,
    }, {
        .data = "7",
        .data_len = 1,
    }, {
        .data = "8",
        .data_len = 1,
    } };


    rc = nffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 8);

    nffs_gc(NULL);

    nffs_test_util_assert_block_count("/myfile.txt", 1);
}

TEST_CASE(nffs_test_wear_level)
{
    int rc;
    int i;
    int j;

    static const struct nffs_area_desc area_descs_uniform[] = {
        { 0x00000000, 2 * 1024 },
        { 0x00020000, 2 * 1024 },
        { 0x00040000, 2 * 1024 },
        { 0x00060000, 2 * 1024 },
        { 0x00080000, 2 * 1024 },
        { 0, 0 },
    };


    /*** Setup. */
    rc = nffs_format(area_descs_uniform);
    TEST_ASSERT(rc == 0);

    /* Ensure areas rotate properly. */
    for (i = 0; i < 255; i++) {
        for (j = 0; j < nffs_num_areas; j++) {
            nffs_test_assert_area_seqs(i, nffs_num_areas - j, i + 1, j);
            nffs_gc(NULL);
        }
    }

    /* Ensure proper rollover of sequence numbers. */
    for (j = 0; j < nffs_num_areas; j++) {
        nffs_test_assert_area_seqs(255, nffs_num_areas - j, 0, j);
        nffs_gc(NULL);
    }
    for (j = 0; j < nffs_num_areas; j++) {
        nffs_test_assert_area_seqs(0, nffs_num_areas - j, 1, j);
        nffs_gc(NULL);
    }
}

TEST_CASE(nffs_test_corrupt_scratch)
{
    int non_scratch_id;
    int scratch_id;
    int rc;

    static const struct nffs_area_desc area_descs_two[] = {
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0, 0 },
    };


    /*** Setup. */
    rc = nffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/myfile.txt", "contents", 8);

    /* Copy the current contents of the non-scratch area to the scratch area.
     * This will make the scratch area look like it only partially participated
     * in a garbage collection cycle.
     */
    scratch_id = nffs_scratch_area_idx;
    non_scratch_id = scratch_id ^ 1;
    nffs_test_copy_area(area_descs_two + non_scratch_id,
                       area_descs_two + nffs_scratch_area_idx);

    /* Add some more data to the non-scratch area. */
    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    /* Ensure the file system is successfully detected and valid, despite
     * corruption.
     */

    rc = nffs_misc_reset();
    TEST_ASSERT(rc == 0);

    rc = nffs_detect(area_descs_two);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(nffs_scratch_area_idx == scratch_id);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "mydir",
                .is_dir = 1,
            }, {
                .filename = "myfile.txt",
                .contents = "contents",
                .contents_len = 8,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, area_descs_two);
}

/*
 * This test no longer works with the current implementation. The
 * expectation is that intermediate blocks can be removed and the old
 * method of finding the last current block after restore will allow the
 * file to be salvaged. Instead, the file should be removed and all data
 * declared invalid.
 */
TEST_CASE(nffs_test_incomplete_block)
{
    struct nffs_block block;
    struct fs_file *fs_file;
    struct nffs_file *file;
    uint32_t flash_offset;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/mydir/a", "aaaa", 4);
    nffs_test_util_create_file("/mydir/b", "bbbb", 4);
    nffs_test_util_create_file("/mydir/c", "cccc", 4);

    /* Add a second block to the 'b' file. */
    nffs_test_util_append_file("/mydir/b", "1234", 4);

    /* Corrupt the 'b' file; make it look like the second block only got half
     * written.
     */
    rc = fs_open("/mydir/b", FS_ACCESS_READ, &fs_file);
    TEST_ASSERT(rc == 0);
    file = (struct nffs_file *)fs_file;

    rc = nffs_block_from_hash_entry(&block,
                                   file->nf_inode_entry->nie_last_block_entry);
    TEST_ASSERT(rc == 0);

    nffs_flash_loc_expand(block.nb_hash_entry->nhe_flash_loc, &area_idx,
                         &area_offset);
    flash_offset = nffs_areas[area_idx].na_offset + area_offset;
    /*
     * Overwrite block data - the CRC check should pick this up
     */
    rc = flash_native_memset(
            flash_offset + sizeof (struct nffs_disk_block) + 2, 0xff, 2);
    TEST_ASSERT(rc == 0);

    rc = nffs_misc_reset();
    TEST_ASSERT(rc == 0);
    rc = nffs_detect(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /* OLD: The entire second block should be removed; the file should only
     * contain the first block.
     * Unless we can salvage the block, the entire file should probably be
     * removed. This is a contrived example which generates bad data on the
     * what happens to be the last block, but corruption can actually occur
     * in any block. Sweep should be updated to search look for blocks that
     * don't have a correct prev_id and then decide whether to delete the
     * owning inode. XXX
     */
    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "mydir",
                .is_dir = 1,
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = "a",
                    .contents = "aaaa",
                    .contents_len = 4,
#if 0
/* keep this out until sweep updated to capture bad blocks XXX */
                }, {
                    .filename = "b",
                    .contents = "bbbb",
                    .contents_len = 4,
#endif
                }, {
                    .filename = "c",
                    .contents = "cccc",
                    .contents_len = 4,
                }, {
                    .filename = NULL,
                } },
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_corrupt_block)
{
    struct nffs_block block;
    struct fs_file *fs_file;
    struct nffs_file *file;
    uint32_t flash_offset;
    uint32_t area_offset;
    uint8_t area_idx;
    uint8_t off;    /* offset to corrupt */
    int rc;
    struct nffs_disk_block ndb;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/mydir/a", "aaaa", 4);
    nffs_test_util_create_file("/mydir/b", "bbbb", 4);
    nffs_test_util_create_file("/mydir/c", "cccc", 4);

    /* Add a second block to the 'b' file. */
    nffs_test_util_append_file("/mydir/b", "1234", 4);

    /* Corrupt the 'b' file; overwrite the second block's magic number. */
    rc = fs_open("/mydir/b", FS_ACCESS_READ, &fs_file);
    TEST_ASSERT(rc == 0);
    file = (struct nffs_file *)fs_file;

    rc = nffs_block_from_hash_entry(&block,
                                   file->nf_inode_entry->nie_last_block_entry);
    TEST_ASSERT(rc == 0);

    nffs_flash_loc_expand(block.nb_hash_entry->nhe_flash_loc, &area_idx,
                         &area_offset);
    flash_offset = nffs_areas[area_idx].na_offset + area_offset;

    /*
     * Overwriting the reserved16 field should invalidate the CRC
     */
    off = (char*)&ndb.reserved16 - (char*)&ndb;
    rc = flash_native_memset(flash_offset + off, 0x43, 1);

    TEST_ASSERT(rc == 0);

    /* Write a fourth file. This file should get restored even though the
     * previous object has an invalid magic number.
     */
    nffs_test_util_create_file("/mydir/d", "dddd", 4);

    rc = nffs_misc_reset();
    TEST_ASSERT(rc == 0);
    rc = nffs_detect(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /* The entire second block should be removed; the file should only contain
     * the first block.
     */
    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "mydir",
                .is_dir = 1,
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = "a",
                    .contents = "aaaa",
                    .contents_len = 4,
#if 0
                /*
                 * In the newer implementation without the find_file_ends
                 * corrupted inodes are deleted rather than retained with
                 * partial contents
                 */
                }, {
                    .filename = "b",
                    .contents = "bbbb",
                    .contents_len = 4,
#endif
                }, {
                    .filename = "c",
                    .contents = "cccc",
                    .contents_len = 4,
                }, {
                    .filename = "d",
                    .contents = "dddd",
                    .contents_len = 4,
                }, {
                    .filename = NULL,
                } },
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_large_unlink)
{
    static char file_contents[1024 * 4];
    char filename[256];
    int rc;
    int i;
    int j;
    int k;


    /*** Setup. */
    nffs_config.nc_num_inodes = 1024;
    nffs_config.nc_num_blocks = 1024;

    rc = nffs_init();
    TEST_ASSERT(rc == 0);

    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < 5; i++) {
        snprintf(filename, sizeof filename, "/dir0_%d", i);
        rc = fs_mkdir(filename);
        TEST_ASSERT(rc == 0);

        for (j = 0; j < 5; j++) {
            snprintf(filename, sizeof filename, "/dir0_%d/dir1_%d", i, j);
            rc = fs_mkdir(filename);
            TEST_ASSERT(rc == 0);

            for (k = 0; k < 5; k++) {
                snprintf(filename, sizeof filename,
                         "/dir0_%d/dir1_%d/file2_%d", i, j, k);
                nffs_test_util_create_file(filename, file_contents,
                                          sizeof file_contents);
            }
        }

        for (j = 0; j < 15; j++) {
            snprintf(filename, sizeof filename, "/dir0_%d/file1_%d", i, j);
            nffs_test_util_create_file(filename, file_contents,
                                      sizeof file_contents);
        }
    }

    for (i = 0; i < 5; i++) {
        snprintf(filename, sizeof filename, "/dir0_%d", i);
        rc = fs_unlink(filename);
        TEST_ASSERT(rc == 0);
    }

    /* The entire file system should be empty. */
    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_large_system)
{
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);
    nffs_test_util_create_tree(nffs_test_system_01);

    nffs_test_assert_system(nffs_test_system_01, nffs_area_descs);

    rc = fs_unlink("/lvl1dir-0000");
    TEST_ASSERT(rc == 0);

    rc = fs_unlink("/lvl1dir-0004");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/lvl1dir-0000");
    TEST_ASSERT(rc == 0);

    nffs_test_assert_system(nffs_test_system_01_rm_1014_mk10, nffs_area_descs);
}

TEST_CASE(nffs_test_lost_found)
{
    char buf[32];
    struct nffs_inode_entry *inode_entry;
    uint32_t flash_offset;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;
    struct nffs_disk_inode ndi;
    uint8_t off;    /* calculated offset for memset */

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);
    rc = fs_mkdir("/mydir/dir1");
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/mydir/file1", "aaaa", 4);
    nffs_test_util_create_file("/mydir/dir1/file2", "bbbb", 4);

    /* Corrupt the mydir inode. */
    rc = nffs_path_find_inode_entry("/mydir", &inode_entry);
    TEST_ASSERT(rc == 0);

    snprintf(buf, sizeof buf, "%lu",
             (unsigned long)inode_entry->nie_hash_entry.nhe_id);

    nffs_flash_loc_expand(inode_entry->nie_hash_entry.nhe_flash_loc,
                         &area_idx, &area_offset);
    flash_offset = nffs_areas[area_idx].na_offset + area_offset;
    /*
     * Overwrite the sequence number - should be detected as CRC corruption
     */
    off = (char*)&ndi.ndi_seq - (char*)&ndi;
    rc = flash_native_memset(flash_offset + off, 0xaa, 1);
    TEST_ASSERT(rc == 0);

    /* Clear cached data and restore from flash (i.e, simulate a reboot). */
    rc = nffs_misc_reset();
    TEST_ASSERT(rc == 0);
    rc = nffs_detect(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /* All contents should now be in the lost+found dir. */
    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "lost+found",
                .is_dir = 1,
#if 0
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = buf,
                    .is_dir = 1,
                    .children = (struct nffs_test_file_desc[]) { {
                        .filename = "file1",
                        .contents = "aaaa",
                        .contents_len = 4,
                    }, {
                        .filename = "dir1",
                        .is_dir = 1,
                        .children = (struct nffs_test_file_desc[]) { {
                            .filename = "file2",
                            .contents = "bbbb",
                            .contents_len = 4,
                        }, {
                            .filename = NULL,
                        } },
                    }, {
                        .filename = NULL,
                    } },
                }, {
                    .filename = NULL,
                } },
#endif
            }, {
                .filename = NULL,
            } }
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_cache_large_file)
{
    static char data[NFFS_BLOCK_MAX_DATA_SZ_MAX * 5];
    struct fs_file *file;
    uint8_t b;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/myfile.txt", data, sizeof data);
    nffs_cache_clear();

    /* Opening a file should not cause any blocks to get cached. */
    rc = fs_open("/myfile.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt", 0, 0);

    /* Cache first block. */
    rc = fs_seek(file, nffs_block_max_data_sz * 0);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 0,
                                     nffs_block_max_data_sz * 1);

    /* Cache second block. */
    rc = fs_seek(file, nffs_block_max_data_sz * 1);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 0,
                                     nffs_block_max_data_sz * 2);


    /* Cache fourth block; prior cache should get erased. */
    rc = fs_seek(file, nffs_block_max_data_sz * 3);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 3,
                                     nffs_block_max_data_sz * 4);

    /* Cache second and third blocks. */
    rc = fs_seek(file, nffs_block_max_data_sz * 1);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 1,
                                     nffs_block_max_data_sz * 4);

    /* Cache fifth block. */
    rc = fs_seek(file, nffs_block_max_data_sz * 4);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 1,
                                     nffs_block_max_data_sz * 5);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(nffs_test_readdir)
{
    struct fs_dirent *dirent;
    struct fs_dir *dir;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT_FATAL(rc == 0);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT_FATAL(rc == 0);

    nffs_test_util_create_file("/mydir/b", "bbbb", 4);
    nffs_test_util_create_file("/mydir/a", "aaaa", 4);
    rc = fs_mkdir("/mydir/c");
    TEST_ASSERT_FATAL(rc == 0);

    /* Nonexistent directory. */
    rc = fs_opendir("/asdf", &dir);
    TEST_ASSERT(rc == FS_ENOENT);

    /* Fail to opendir a file. */
    rc = fs_opendir("/mydir/a", &dir);
    TEST_ASSERT(rc == FS_EINVAL);

    /* Real directory (with trailing slash). */
    rc = fs_opendir("/mydir/", &dir);
    TEST_ASSERT_FATAL(rc == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "a");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "b");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "c");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 1);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_closedir(dir);
    TEST_ASSERT(rc == 0);

    /* Root directory. */
    rc = fs_opendir("/", &dir);
    TEST_ASSERT(rc == 0);
    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_ent_name(dirent, "lost+found");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 1);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "mydir");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 1);

    rc = fs_closedir(dir);
    TEST_ASSERT(rc == 0);

    /* Delete entries while iterating. */
    rc = fs_opendir("/mydir", &dir);
    TEST_ASSERT_FATAL(rc == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_ent_name(dirent, "a");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 0);

    rc = fs_unlink("/mydir/b");
    TEST_ASSERT(rc == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);

    rc = fs_unlink("/mydir/c");
    TEST_ASSERT(rc == 0);

    rc = fs_unlink("/mydir");
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_ent_name(dirent, "c");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 1);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_closedir(dir);
    TEST_ASSERT(rc == 0);

    /* Ensure directory is gone. */
    rc = fs_opendir("/mydir", &dir);
    TEST_ASSERT(rc == FS_ENOENT);
}

TEST_CASE(nffs_test_split_file)
{
    static char data[24 * 1024];
    int rc;
    int i;

    /*** Setup. */
    static const struct nffs_area_desc area_descs_two[] = {
            { 0x00000000, 16 * 1024 },
            { 0x00004000, 16 * 1024 },
            { 0x00008000, 16 * 1024 },
            { 0, 0 },
    };

    rc = nffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof data; i++) {
        data[i] = i;
    }

    for (i = 0; i < 256; i++) {
        nffs_test_util_create_file("/myfile.txt", data, sizeof data);
        rc = fs_unlink("/myfile.txt");
        TEST_ASSERT(rc == 0);
    }

    nffs_test_util_create_file("/myfile.txt", data, sizeof data);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = data,
                .contents_len = sizeof data,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, area_descs_two);
}

TEST_CASE(nffs_test_gc_on_oom)
{
    int rc;

    /*** Setup. */
    /* Ensure all areas are the same size. */
    static const struct nffs_area_desc area_descs_two[] = {
            { 0x00000000, 16 * 1024 },
            { 0x00004000, 16 * 1024 },
            { 0x00008000, 16 * 1024 },
            { 0, 0 },
    };

    rc = nffs_format(area_descs_two);
    TEST_ASSERT_FATAL(rc == 0);

    /* Leak block entries until only four are left. */
    /* XXX: This is ridiculous.  Need to fix nffs configuration so that the
     * caller passes a config object rather than writing to a global variable.
     */
    while (nffs_block_entry_pool.mp_num_free != 4) {
        nffs_block_entry_alloc();
    }

    /*** Write 4 data blocks. */
    struct nffs_test_block_desc blocks[4] = { {
        .data = "1",
        .data_len = 1,
    }, {
        .data = "2",
        .data_len = 1,
    }, {
        .data = "3",
        .data_len = 1,
    }, {
        .data = "4",
        .data_len = 1,
    } };

    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 4);

    TEST_ASSERT_FATAL(nffs_block_entry_pool.mp_num_free == 0);

    /* Attempt another one-byte write.  This should trigger a garbage
     * collection cycle, resulting in the four blocks being collated.  The
     * fifth write consumes an additional block, resulting in 2 out of 4 blocks
     * in use.
     */
    nffs_test_util_append_file("/myfile.txt", "5", 1);

    TEST_ASSERT_FATAL(nffs_block_entry_pool.mp_num_free == 2);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "12345",
                .contents_len = 5,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, area_descs_two);
}

TEST_SUITE(nffs_suite_cache)
{
    int rc;

    memset(&nffs_config, 0, sizeof nffs_config);
    nffs_config.nc_num_cache_inodes = 4;
    nffs_config.nc_num_cache_blocks = 64;

    rc = nffs_init();
    TEST_ASSERT(rc == 0);

    nffs_test_cache_large_file();
}

static void
nffs_test_gen(void)
{
    int rc;

    rc = nffs_init();
    TEST_ASSERT(rc == 0);

    nffs_test_unlink();
    nffs_test_mkdir();
    nffs_test_rename();
    nffs_test_truncate();
    nffs_test_append();
    nffs_test_read();
    nffs_test_open();
    nffs_test_overwrite_one();
    nffs_test_overwrite_two();
    nffs_test_overwrite_three();
    nffs_test_overwrite_many();
    nffs_test_long_filename();
    nffs_test_large_write();
    nffs_test_many_children();
    nffs_test_gc();
    nffs_test_wear_level();
    nffs_test_corrupt_scratch();
    nffs_test_incomplete_block();
    nffs_test_corrupt_block();
    nffs_test_large_unlink();
    nffs_test_large_system();
    nffs_test_lost_found();
    nffs_test_readdir();
    nffs_test_split_file();
    nffs_test_gc_on_oom();
}

TEST_SUITE(gen_1_1)
{
    nffs_config.nc_num_cache_inodes = 1;
    nffs_config.nc_num_cache_blocks = 1;
    nffs_test_gen();
}

TEST_SUITE(gen_4_32)
{
    nffs_config.nc_num_cache_inodes = 4;
    nffs_config.nc_num_cache_blocks = 32;
    nffs_test_gen();
}

TEST_SUITE(gen_32_1024)
{
    nffs_config.nc_num_cache_inodes = 32;
    nffs_config.nc_num_cache_blocks = 1024;
    nffs_test_gen();
}

int
nffs_test_all(void)
{
    nffs_config.nc_num_inodes = 1024 * 8;
    nffs_config.nc_num_blocks = 1024 * 20;

    gen_1_1();
    gen_4_32();
    gen_32_1024();
    nffs_suite_cache();

    return tu_any_failed;
}

void
print_inode_entry(struct nffs_inode_entry *inode_entry, int indent)
{
    struct nffs_inode inode;
    char name[NFFS_FILENAME_MAX_LEN + 1];
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    if (inode_entry == nffs_root_dir) {
        printf("%*s/\n", indent, "");
        return;
    }

    rc = nffs_inode_from_entry(&inode, inode_entry);
    /*
     * Dummy inode
     */
    if (rc == FS_ENOENT) {
        printf("    DUMMY %d\n", rc);
        return;
    }

    nffs_flash_loc_expand(inode_entry->nie_hash_entry.nhe_flash_loc,
                         &area_idx, &area_offset);

    rc = nffs_flash_read(area_idx,
                         area_offset + sizeof (struct nffs_disk_inode),
                         name, inode.ni_filename_len);

    name[inode.ni_filename_len] = '\0';

    printf("%*s%s\n", indent, "", name[0] == '\0' ? "/" : name);
}

void
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

int
print_nffs_flash_inode(struct nffs_area *area, uint32_t off)
{
    struct nffs_disk_inode ndi;
    char filename[128];
    int len;
    int rc;

    rc = hal_flash_read(area->na_flash_id, area->na_offset + off,
                         &ndi, sizeof(ndi));
    assert(rc == 0);

    memset(filename, 0, sizeof(filename));
    len = min(sizeof(filename) - 1, ndi.ndi_filename_len);
    rc = hal_flash_read(area->na_flash_id, area->na_offset + off + sizeof(ndi),
                         filename, len);

    printf("  off %x %s id %x flen %d seq %d last %x prnt %x flgs %x %s\n",
           off,
           (nffs_hash_id_is_file(ndi.ndi_id) ? "File" :
            (nffs_hash_id_is_dir(ndi.ndi_id) ? "Dir" : "???")),
           ndi.ndi_id,
           ndi.ndi_filename_len,
           ndi.ndi_seq,
           ndi.ndi_lastblock_id,
           ndi.ndi_parent_id,
           ndi.ndi_flags,
           filename);
    return sizeof(ndi) + ndi.ndi_filename_len;
}

int
print_nffs_flash_block(struct nffs_area *area, uint32_t off)
{
    struct nffs_disk_block ndb;
    int rc;

    rc = hal_flash_read(area->na_flash_id, area->na_offset + off,
                        &ndb, sizeof(ndb));
    assert(rc == 0);

    printf("  off %x Block id %x len %d seq %d prev %x own ino %x\n",
           off,
           ndb.ndb_id,
           ndb.ndb_data_len,
           ndb.ndb_seq,
           ndb.ndb_prev_id,
           ndb.ndb_inode_id);
    return sizeof(ndb) + ndb.ndb_data_len;
}

int
print_nffs_flash_object(struct nffs_area *area, uint32_t off)
{
    struct nffs_disk_object ndo;

    hal_flash_read(area->na_flash_id, area->na_offset + off,
                        &ndo.ndo_un_obj, sizeof(ndo.ndo_un_obj));

    if (nffs_hash_id_is_inode(ndo.ndo_disk_inode.ndi_id)) {
        return print_nffs_flash_inode(area, off);

    } else if (nffs_hash_id_is_block(ndo.ndo_disk_block.ndb_id)) {
        return print_nffs_flash_block(area, off);

    } else if (ndo.ndo_disk_block.ndb_id == 0xffffffff) {
        return area->na_length;

    } else {
        return 1;
    }
}

void
print_nffs_flash_areas(int verbose)
{
    struct nffs_area area;
    struct nffs_disk_area darea;
    int off;
    int i;

    for (i = 0; nffs_current_area_descs[i].nad_length != 0; i++) {
        if (i > NFFS_MAX_AREAS) {
            return;
        }
        area.na_offset = nffs_current_area_descs[i].nad_offset;
        area.na_length = nffs_current_area_descs[i].nad_length;
        area.na_flash_id = nffs_current_area_descs[i].nad_flash_id;
        hal_flash_read(area.na_flash_id, area.na_offset, &darea, sizeof(darea));
        area.na_id = darea.nda_id;
        area.na_cur = nffs_areas[i].na_cur;
        if (!nffs_area_magic_is_set(&darea)) {
            printf("Area header corrupt!\n");
        }
        printf("area %d: id %d %x-%x cur %x len %d flashid %x gc-seq %d %s%s\n",
               i, area.na_id, area.na_offset, area.na_offset + area.na_length,
               area.na_cur, area.na_length, area.na_flash_id, darea.nda_gc_seq,
               nffs_scratch_area_idx == i ? "(scratch)" : "",
               !nffs_area_magic_is_set(&darea) ? "corrupt" : "");
        if (verbose < 2) {
            off = sizeof (struct nffs_disk_area);
            while (off < area.na_length) {
                off += print_nffs_flash_object(&area, off);
            }
        }
    }
}

static int
nffs_hash_fn(uint32_t id)
{
    return id % NFFS_HASH_SIZE;
}

void
print_hashlist(struct nffs_hash_entry *he)
{
    struct nffs_hash_list *list;
    int idx = nffs_hash_fn(he->nhe_id);
    list = nffs_hash + idx;

    SLIST_FOREACH(he, list, nhe_next) {
        printf("hash_entry %s 0x%x: id 0x%x flash_loc 0x%x next 0x%x\n",
                   nffs_hash_id_is_inode(he->nhe_id) ? "inode" : "block",
                   (unsigned int)he,
                   he->nhe_id, he->nhe_flash_loc,
                   (unsigned int)he->nhe_next.sle_next);
   }
}

void
print_hash(void)
{
    int i;
    struct nffs_hash_entry *he;
    struct nffs_hash_entry *next;
    struct nffs_inode ni;
    struct nffs_disk_inode di;
    struct nffs_block nb;
    struct nffs_disk_block db;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    NFFS_HASH_FOREACH(he, i, next) {
        if (nffs_hash_id_is_inode(he->nhe_id)) {
            printf("hash_entry inode %d 0x%x: id 0x%x flash_loc 0x%x next 0x%x\n",
                   i, (unsigned int)he,
                   he->nhe_id, he->nhe_flash_loc,
                   (unsigned int)he->nhe_next.sle_next);
            if (he->nhe_id == NFFS_ID_ROOT_DIR) {
                continue;
            }
            nffs_flash_loc_expand(he->nhe_flash_loc,
                                  &area_idx, &area_offset);
            rc = nffs_inode_read_disk(area_idx, area_offset, &di);
            if (rc) {
                printf("%d: fail inode read id 0x%x rc %d\n",
                       i, he->nhe_id, rc);
            }
            printf("    Disk inode: id %x seq %d parent %x last %x flgs %x\n",
                   di.ndi_id,
                   di.ndi_seq,
                   di.ndi_parent_id,
                   di.ndi_lastblock_id,
                   di.ndi_flags);
            ni.ni_inode_entry = (struct nffs_inode_entry *)he;
            ni.ni_seq = di.ndi_seq; 
            ni.ni_parent = nffs_hash_find_inode(di.ndi_parent_id);
            printf("    RAM inode: entry 0x%x seq %d parent %x filename %s\n",
                   (unsigned int)ni.ni_inode_entry,
                   ni.ni_seq,
                   (unsigned int)ni.ni_parent,
                   ni.ni_filename);

        } else if (nffs_hash_id_is_block(he->nhe_id)) {
            printf("hash_entry block %d 0x%x: id 0x%x flash_loc 0x%x next 0x%x\n",
                   i, (unsigned int)he,
                   he->nhe_id, he->nhe_flash_loc,
                   (unsigned int)he->nhe_next.sle_next);
            rc = nffs_block_from_hash_entry(&nb, he);
            if (rc) {
                printf("%d: fail block read id 0x%x rc %d\n",
                       i, he->nhe_id, rc);
            }
            printf("    block: id %x seq %d inode %x prev %x\n",
                   nb.nb_hash_entry->nhe_id, nb.nb_seq, 
                   nb.nb_inode_entry->nie_hash_entry.nhe_id, 
                   nb.nb_prev->nhe_id);
            nffs_flash_loc_expand(nb.nb_hash_entry->nhe_flash_loc,
                                  &area_idx, &area_offset);
            rc = nffs_block_read_disk(area_idx, area_offset, &db);
            if (rc) {
                printf("%d: fail disk block read id 0x%x rc %d\n",
                       i, nb.nb_hash_entry->nhe_id, rc);
            }
            printf("    disk block: id %x seq %d inode %x prev %x len %d\n",
                   db.ndb_id,
                   db.ndb_seq,
                   db.ndb_inode_id,
                   db.ndb_prev_id,
                   db.ndb_data_len);
        } else {
            printf("hash_entry UNKNONN %d 0x%x: id 0x%x flash_loc 0x%x next 0x%x\n",
                   i, (unsigned int)he,
                   he->nhe_id, he->nhe_flash_loc,
                   (unsigned int)he->nhe_next.sle_next);
        }
    }

}

void
nffs_print_object(struct nffs_disk_object *dobj)
{
    struct nffs_disk_inode *di = &dobj->ndo_disk_inode;
    struct nffs_disk_block *db = &dobj->ndo_disk_block;

    if (dobj->ndo_type == NFFS_OBJECT_TYPE_INODE) {
        printf("    %s id %x seq %d prnt %x last %x\n",
               nffs_hash_id_is_file(di->ndi_id) ? "File" :
                nffs_hash_id_is_dir(di->ndi_id) ? "Dir" : "???",
               di->ndi_id, di->ndi_seq, di->ndi_parent_id,
               di->ndi_lastblock_id);
    } else if (dobj->ndo_type != NFFS_OBJECT_TYPE_BLOCK) {
        printf("    %s: id %x seq %d ino %x prev %x len %d\n",
               nffs_hash_id_is_block(db->ndb_id) ? "Block" : "Block?",
               db->ndb_id, db->ndb_seq, db->ndb_inode_id,
               db->ndb_prev_id, db->ndb_data_len);
    }
}

void
print_nffs_hash_block(struct nffs_hash_entry *he, int verbose)
{
    struct nffs_block nb;
    struct nffs_disk_block db;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    if (he == NULL) {
        return;
    }
    if (!nffs_hash_entry_is_dummy(he)) {
        nffs_flash_loc_expand(he->nhe_flash_loc,
                              &area_idx, &area_offset);
        rc = nffs_block_read_disk(area_idx, area_offset, &db);
        if (rc) {
            printf("%p: fail block read id 0x%x rc %d\n",
                   he, he->nhe_id, rc);
        }
        nb.nb_hash_entry = he;
        nb.nb_seq = db.ndb_seq;
        if (db.ndb_inode_id != NFFS_ID_NONE) {
            nb.nb_inode_entry = nffs_hash_find_inode(db.ndb_inode_id);
        } else {
            nb.nb_inode_entry = (void*)db.ndb_inode_id;
        }
        if (db.ndb_prev_id != NFFS_ID_NONE) {
            nb.nb_prev = nffs_hash_find_block(db.ndb_prev_id);
        } else {
            nb.nb_prev = (void*)db.ndb_prev_id;
        }
        nb.nb_data_len = db.ndb_data_len;
    } else {
        nb.nb_inode_entry = NULL;
        db.ndb_id = 0;
    }
    if (!verbose) {
        printf("%s%s id %x idx/off %d/%x seq %d ino %x prev %x len %d\n",
               nffs_hash_entry_is_dummy(he) ? "Dummy " : "",
               nffs_hash_id_is_block(he->nhe_id) ? "Block" : "Unknown",
               he->nhe_id, area_idx, area_offset, nb.nb_seq,
               nb.nb_inode_entry->nie_hash_entry.nhe_id,
               (unsigned int)db.ndb_prev_id, db.ndb_data_len);
        return;
    }
    printf("%s%s id %x loc %x/%x %x ent %p\n",
           nffs_hash_entry_is_dummy(he) ? "Dummy " : "",
           nffs_hash_id_is_block(he->nhe_id) ? "Block:" : "Unknown:",
           he->nhe_id, area_idx, area_offset, he->nhe_flash_loc, he);
    if (nb.nb_inode_entry) {
        printf("  Ram: ent %p seq %d ino %p prev %p len %d\n",
               nb.nb_hash_entry, nb.nb_seq,
               nb.nb_inode_entry, nb.nb_prev, nb.nb_data_len);
    }
    if (db.ndb_id) {
        printf("  Disk %s id %x seq %d ino %x prev %x len %d\n",
               nffs_hash_id_is_block(db.ndb_id) ? "Block:" : "???:",
               db.ndb_id, db.ndb_seq, db.ndb_inode_id,
               db.ndb_prev_id, db.ndb_data_len);
    }
}

void
print_nffs_hash_inode(struct nffs_hash_entry *he, int verbose)
{
    struct nffs_inode ni;
    struct nffs_disk_inode di;
    struct nffs_inode_entry *nie = (struct nffs_inode_entry*)he;
    int cached_name_len;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    if (he == NULL) {
        return;
    }
    if (!nffs_hash_entry_is_dummy(he)) {
        nffs_flash_loc_expand(he->nhe_flash_loc,
                              &area_idx, &area_offset);
        rc = nffs_inode_read_disk(area_idx, area_offset, &di);
        if (rc) {
            printf("Entry %p: fail inode read id 0x%x rc %d\n",
                   he, he->nhe_id, rc);
        }
        ni.ni_inode_entry = (struct nffs_inode_entry *)he;
        ni.ni_seq = di.ndi_seq; 
        if (di.ndi_parent_id != NFFS_ID_NONE) {
            ni.ni_parent = nffs_hash_find_inode(di.ndi_parent_id);
        } else {
            ni.ni_parent = NULL;
        }
        if (ni.ni_filename_len > NFFS_SHORT_FILENAME_LEN) {
            cached_name_len = NFFS_SHORT_FILENAME_LEN;
        } else {
            cached_name_len = ni.ni_filename_len;
        }
        if (cached_name_len != 0) {
            rc = nffs_flash_read(area_idx, area_offset + sizeof di,
                         ni.ni_filename, cached_name_len);
            if (rc != 0) {
                printf("entry %p: fail filename read id 0x%x rc %d\n",
                       he, he->nhe_id, rc);
                return;
            }
        }
    } else {
        ni.ni_inode_entry = NULL;
        di.ndi_id = 0;
    }
    if (!verbose) {
        printf("%s%s id %x idx/off %x/%x seq %d prnt %x last %x flags %x",
               nffs_hash_entry_is_dummy(he) ? "Dummy " : "",

               nffs_hash_id_is_file(he->nhe_id) ? "File" :
                he->nhe_id == NFFS_ID_ROOT_DIR ? "**ROOT Dir" : 
                nffs_hash_id_is_dir(he->nhe_id) ? "Dir" : "Inode",

               he->nhe_id, area_idx, area_offset, ni.ni_seq, di.ndi_parent_id,
               di.ndi_lastblock_id, nie->nie_flags);
        if (ni.ni_inode_entry) {
            printf(" ref %d\n", ni.ni_inode_entry->nie_refcnt);
        } else {
            printf("\n");
        }
        return;
    }
    printf("%s%s id %x loc %x/%x %x entry %p\n",
           nffs_hash_entry_is_dummy(he) ? "Dummy " : "",
           nffs_hash_id_is_file(he->nhe_id) ? "File:" :
            he->nhe_id == NFFS_ID_ROOT_DIR ? "**ROOT Dir:" : 
            nffs_hash_id_is_dir(he->nhe_id) ? "Dir:" : "Inode:",
           he->nhe_id, area_idx, area_offset, he->nhe_flash_loc, he);
    if (ni.ni_inode_entry) {
        printf("  ram: ent %p seq %d prnt %p lst %p ref %d flgs %x nm %s\n",
               ni.ni_inode_entry, ni.ni_seq, ni.ni_parent,
               ni.ni_inode_entry->nie_last_block_entry,
               ni.ni_inode_entry->nie_refcnt, ni.ni_inode_entry->nie_flags,
               ni.ni_filename);
    }
    if (rc == 0) {
        printf("  Disk %s: id %x seq %d prnt %x lst %x flgs %x\n",
               nffs_hash_id_is_file(di.ndi_id) ? "File" :
                nffs_hash_id_is_dir(di.ndi_id) ? "Dir" : "???",
               di.ndi_id, di.ndi_seq, di.ndi_parent_id,
               di.ndi_lastblock_id, di.ndi_flags);
    }
}

void
print_hash_entries(int verbose)
{
    int i;
    struct nffs_hash_entry *he;
    struct nffs_hash_entry *next;

    printf("\nnffs_hash_entries:\n");
    for (i = 0; i < NFFS_HASH_SIZE; i++) {
        he = SLIST_FIRST(nffs_hash + i);
        while (he != NULL) {
            next = SLIST_NEXT(he, nhe_next);
            if (nffs_hash_id_is_inode(he->nhe_id)) {
                print_nffs_hash_inode(he, verbose);
            } else if (nffs_hash_id_is_block(he->nhe_id)) {
                print_nffs_hash_block(he, verbose);
            } else {
                printf("UNKNOWN type hash entry %d: id 0x%x loc 0x%x\n",
                       i, he->nhe_id, he->nhe_flash_loc);
            }
            he = next;
        }
    }
}

void
print_nffs_hashlist(int verbose)
{
    struct nffs_hash_entry *he;
    struct nffs_hash_entry *next;
    int i;

    NFFS_HASH_FOREACH(he, i, next) {
        if (nffs_hash_id_is_inode(he->nhe_id)) {
            print_nffs_hash_inode(he, verbose);
        } else if (nffs_hash_id_is_block(he->nhe_id)) {
            print_nffs_hash_block(he, verbose);
        } else {
            printf("UNKNOWN type hash entry %d: id 0x%x loc 0x%x\n",
                   i, he->nhe_id, he->nhe_flash_loc);
        }
    }
}

static int print_verbose;

void
printfs()
{
    if (nffs_misc_ready()) {
        printf("NFFS directory:\n");
        process_inode_entry(nffs_root_dir, print_verbose);

        printf("\nNFFS hash list:\n");
        print_nffs_hashlist(print_verbose);
    }
    printf("\nNFFS flash areas:\n");
    print_nffs_flash_areas(print_verbose);
}

#include <unistd.h>

#if 0
void
nffs_assert_handler(const char *file, int line, const char *func, const char *e)
{
    char msg[256];

    snprintf(msg, sizeof(msg), "assert at %s:%d\n", file, line);
    write(1, msg, strlen(msg));
    _exit(1);
}
#endif

#ifdef MYNEWT_SELFTEST

int
main(void)
{
    tu_config.tc_print_results = 1;
    tu_init();

    print_verbose = 1;

    nffs_test_all();

    return tu_any_failed;
}

#endif
