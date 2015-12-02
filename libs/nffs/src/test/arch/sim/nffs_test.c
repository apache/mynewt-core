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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "hal/hal_flash.h"
#include "testutil/testutil.h"
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
nffs_test_util_assert_ent_name(struct nffs_dirent *dirent,
                               const char *expected_name)
{
    char name[NFFS_FILENAME_MAX_LEN + 1];
    uint8_t name_len;
    int rc;

    rc = nffs_dirent_name(dirent, sizeof name, name, &name_len);
    TEST_ASSERT(rc == 0);
    if (rc == 0) {
        TEST_ASSERT(strcmp(name, expected_name) == 0);
    }
}

static void
nffs_test_util_assert_file_len(struct nffs_file *file, uint32_t expected)
{
    uint32_t len;
    int rc;

    rc = nffs_file_len(file, &len);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(len == expected);
}

static void
nffs_test_util_assert_cache_is_sane(const char *filename)
{
    struct nffs_cache_inode *cache_inode;
    struct nffs_cache_block *cache_block;
    struct nffs_file *file;
    uint32_t cache_start;
    uint32_t cache_end;
    uint32_t block_end;
    int rc;

    rc = nffs_open(filename, NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

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

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);
}

static void
nffs_test_util_assert_contents(const char *filename, const char *contents,
                              int contents_len)
{
    struct nffs_file *file;
    uint32_t bytes_read;
    void *buf;
    int rc;

    rc = nffs_open(filename, NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    buf = malloc(contents_len + 1);
    TEST_ASSERT(buf != NULL);

    rc = nffs_read(file, contents_len + 1, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == contents_len);
    TEST_ASSERT(memcmp(buf, contents, contents_len) == 0);

    rc = nffs_close(file);
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
    int count;
    int rc;

    rc = nffs_open(filename, NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    count = 0;
    entry = file->nf_inode_entry->nie_last_block_entry;
    while (entry != NULL) {
        count++;
        rc = nffs_block_from_hash_entry(&block, entry);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(block.nb_prev != entry);
        entry = block.nb_prev;
    }

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    return count;
}

static void
nffs_test_util_assert_block_count(const char *filename, int expected_count)
{
    TEST_ASSERT(nffs_test_util_block_count(filename) == expected_count);
}

static void
nffs_test_util_assert_cache_range(const char *filename,
                                 uint32_t expected_cache_start,
                                 uint32_t expected_cache_end)
{
    struct nffs_cache_inode *cache_inode;
    struct nffs_file *file;
    uint32_t cache_start;
    uint32_t cache_end;
    int rc;

    rc = nffs_open(filename, NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    rc = nffs_cache_inode_ensure(&cache_inode, file->nf_inode_entry);
    TEST_ASSERT(rc == 0);

    nffs_cache_inode_range(cache_inode, &cache_start, &cache_end);
    TEST_ASSERT(cache_start == expected_cache_start);
    TEST_ASSERT(cache_end == expected_cache_end);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_cache_is_sane(filename);
}

static void
nffs_test_util_create_file_blocks(const char *filename,
                                 const struct nffs_test_block_desc *blocks,
                                 int num_blocks)
{
    struct nffs_file *file;
    uint32_t total_len;
    uint32_t offset;
    char *buf;
    int num_writes;
    int rc;
    int i;

    rc = nffs_open(filename, NFFS_ACCESS_WRITE | NFFS_ACCESS_TRUNCATE, &file);
    TEST_ASSERT(rc == 0);

    total_len = 0;
    if (num_blocks <= 0) {
        num_writes = 1;
    } else {
        num_writes = num_blocks;
    }
    for (i = 0; i < num_writes; i++) {
        rc = nffs_write(file, blocks[i].data, blocks[i].data_len);
        TEST_ASSERT(rc == 0);

        total_len += blocks[i].data_len;
    }

    rc = nffs_close(file);
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
    struct nffs_file *file;
    int rc;

    rc = nffs_open(filename, NFFS_ACCESS_WRITE | NFFS_ACCESS_APPEND, &file);
    TEST_ASSERT(rc == 0);

    rc = nffs_write(file, contents, contents_len);
    TEST_ASSERT(rc == 0);

    rc = nffs_close(file);
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
            rc = nffs_mkdir(path);
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

    TEST_ASSERT(nffs_test_num_touched_entries < NFFS_TEST_TOUCHED_ARR_SZ);
    nffs_test_touched_entries[nffs_test_num_touched_entries] =
        &inode_entry->nie_hash_entry;
    nffs_test_num_touched_entries++;

    path_len = strlen(path);

    rc = nffs_inode_from_entry(&inode, inode_entry);
    TEST_ASSERT(rc == 0);

    if (nffs_hash_id_is_dir(inode_entry->nie_hash_entry.nhe_id)) {
        for (child_file = file->children;
             child_file != NULL && child_file->filename != NULL;
             child_file++) {

            child_filename_len = strlen(child_file->filename);
            child_path = malloc(path_len + 1 + child_filename_len + 1);
            TEST_ASSERT(child_path != NULL);
            memcpy(child_path, path, path_len);
            child_path[path_len] = '/';
            memcpy(child_path + path_len + 1, child_file->filename,
                   child_filename_len);
            child_path[path_len + 1 + child_filename_len] = '\0';

            rc = nffs_path_find_inode_entry(child_path, &child_inode_entry);
            TEST_ASSERT(rc == 0);

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

    rc = nffs_inode_from_entry(&inode, child);
    TEST_ASSERT(rc == 0);

    parent = inode.ni_parent;
    TEST_ASSERT(parent != NULL);
    TEST_ASSERT(nffs_hash_id_is_dir(parent->nie_hash_entry.nhe_id));

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

    rc = nffs_block_from_hash_entry(&block, block_entry);
    TEST_ASSERT(rc == 0);

    inode_entry = block.nb_inode_entry;
    TEST_ASSERT(inode_entry != NULL);
    TEST_ASSERT(nffs_hash_id_is_file(inode_entry->nie_hash_entry.nhe_id));

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
    int i;

    nffs_test_num_touched_entries = 0;
    nffs_test_assert_file(root_dir, nffs_root_dir, "");
    nffs_test_assert_branch_touched(nffs_root_dir);

    /* Ensure no orphaned inodes or blocks. */
    NFFS_HASH_FOREACH(entry, i) {
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
    struct nffs_file *file;
    int rc;


    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = nffs_mkdir("/a/b/c/d");
    TEST_ASSERT(rc == NFFS_ENOENT);

    rc = nffs_mkdir("asdf");
    TEST_ASSERT(rc == NFFS_EINVAL);

    rc = nffs_mkdir("/a");
    TEST_ASSERT(rc == 0);

    rc = nffs_mkdir("/a/b");
    TEST_ASSERT(rc == 0);

    rc = nffs_mkdir("/a/b/c");
    TEST_ASSERT(rc == 0);

    rc = nffs_mkdir("/a/b/c/d");
    TEST_ASSERT(rc == 0);

    rc = nffs_open("/a/b/c/d/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);

    rc = nffs_close(file);
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
    struct nffs_file *file0;
    struct nffs_file *file1;
    struct nffs_file *file2;
    uint8_t buf[64];
    uint32_t bytes_read;
    int rc;

    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/file0.txt", "0", 1);

    rc = nffs_open("/file0.txt", NFFS_ACCESS_READ | NFFS_ACCESS_WRITE, &file0);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(file0->nf_inode_entry->nie_refcnt == 2);

    rc = nffs_unlink("/file0.txt");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(file0->nf_inode_entry->nie_refcnt == 1);

    rc = nffs_open("/file0.txt", NFFS_ACCESS_READ, &file2);
    TEST_ASSERT(rc == NFFS_ENOENT);

    rc = nffs_write(file0, "00", 2);
    TEST_ASSERT(rc == 0);

    rc = nffs_seek(file0, 0);
    TEST_ASSERT(rc == 0);

    rc = nffs_read(file0, sizeof buf, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 2);
    TEST_ASSERT(memcmp(buf, "00", 2) == 0);

    rc = nffs_close(file0);
    TEST_ASSERT(rc == 0);

    rc = nffs_open("/file0.txt", NFFS_ACCESS_READ, &file0);
    TEST_ASSERT(rc == NFFS_ENOENT);

    /* Nested unlink. */
    rc = nffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);
    nffs_test_util_create_file("/mydir/file1.txt", "1", 2);

    rc = nffs_open("/mydir/file1.txt", NFFS_ACCESS_READ | NFFS_ACCESS_WRITE,
                  &file1);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(file1->nf_inode_entry->nie_refcnt == 2);

    rc = nffs_unlink("/mydir");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(file1->nf_inode_entry->nie_refcnt == 1);

    rc = nffs_open("/mydir/file1.txt", NFFS_ACCESS_READ, &file2);
    TEST_ASSERT(rc == NFFS_ENOENT);

    rc = nffs_write(file1, "11", 2);
    TEST_ASSERT(rc == 0);

    rc = nffs_seek(file1, 0);
    TEST_ASSERT(rc == 0);

    rc = nffs_read(file1, sizeof buf, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 2);
    TEST_ASSERT(memcmp(buf, "11", 2) == 0);

    rc = nffs_close(file1);
    TEST_ASSERT(rc == 0);

    rc = nffs_open("/mydir/file1.txt", NFFS_ACCESS_READ, &file1);
    TEST_ASSERT(rc == NFFS_ENOENT);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_rename)
{
    struct nffs_file *file;
    const char contents[] = "contents";
    int rc;


    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = nffs_rename("/nonexistent.txt", "/newname.txt");
    TEST_ASSERT(rc == NFFS_ENOENT);

    /*** Rename file. */
    nffs_test_util_create_file("/myfile.txt", contents, sizeof contents);

    rc = nffs_rename("/myfile.txt", "badname");
    TEST_ASSERT(rc == NFFS_EINVAL);

    rc = nffs_rename("/myfile.txt", "/myfile2.txt");
    TEST_ASSERT(rc == 0);

    rc = nffs_open("/myfile.txt", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == NFFS_ENOENT);

    nffs_test_util_assert_contents("/myfile2.txt", contents, sizeof contents);

    rc = nffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    rc = nffs_rename("/myfile2.txt", "/mydir/myfile2.txt");
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/mydir/myfile2.txt", contents,
                                  sizeof contents);

    /*** Rename directory. */
    rc = nffs_rename("/mydir", "badname");
    TEST_ASSERT(rc == NFFS_EINVAL);

    rc = nffs_rename("/mydir", "/mydir2");
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
    struct nffs_file *file;
    int rc;


    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE | NFFS_ACCESS_TRUNCATE,
                  &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 8);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abcdefgh", 8);

    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE | NFFS_ACCESS_TRUNCATE,
                  &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_write(file, "1234", 4);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 4);
    TEST_ASSERT(nffs_getpos(file) == 4);
    rc = nffs_close(file);
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
    struct nffs_file *file;
    int rc;


    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE | NFFS_ACCESS_APPEND,
                   &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 8);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abcdefgh", 8);

    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE | NFFS_ACCESS_APPEND,
                   &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 8);

    /* File position should always be at the end of a file after an append.
     * Seek to the middle prior to writing to test this.
     */
    rc = nffs_seek(file, 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 2);

    rc = nffs_write(file, "ijklmnop", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 16);
    rc = nffs_write(file, "qrstuvwx", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 24);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt",
                                  "abcdefghijklmnopqrstuvwx", 24);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdefghijklmnopqrstuvwx",
                .contents_len = 24,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_read)
{
    struct nffs_file *file;
    uint8_t buf[16];
    uint32_t bytes_read;
    int rc;

    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/myfile.txt", "1234567890", 10);

    rc = nffs_open("/myfile.txt", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 10);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_read(file, 4, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 4);
    TEST_ASSERT(memcmp(buf, "1234", 4) == 0);
    TEST_ASSERT(nffs_getpos(file) == 4);

    rc = nffs_read(file, sizeof buf - 4, buf + 4, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 6);
    TEST_ASSERT(memcmp(buf, "1234567890", 10) == 0);
    TEST_ASSERT(nffs_getpos(file) == 10);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(nffs_test_open)
{
    struct nffs_file *file;
    int rc;

    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Fail to open an invalid path (not rooted). */
    rc = nffs_open("file", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == NFFS_EINVAL);

    /*** Fail to open a directory (root directory). */
    rc = nffs_open("/", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == NFFS_EINVAL);

    /*** Fail to open a nonexistent file for reading. */
    rc = nffs_open("/1234", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == NFFS_ENOENT);

    rc = nffs_mkdir("/dir");
    TEST_ASSERT(rc == 0);

    /*** Fail to open a directory. */
    rc = nffs_open("/dir", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == NFFS_EINVAL);

    /*** Successfully open an existing file for reading. */
    nffs_test_util_create_file("/dir/file.txt", "1234567890", 10);
    rc = nffs_open("/dir/file.txt", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    /*** Successfully open an nonexistent file for writing. */
    rc = nffs_open("/dir/file2.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    /*** Ensure the file can be reopened. */
    rc = nffs_open("/dir/file.txt", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(nffs_test_overwrite_one)
{
    struct nffs_file *file;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_append_file("/myfile.txt", "abcdefgh", 8);

    /*** Overwrite within one block (middle). */
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 3);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 3);

    rc = nffs_write(file, "12", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 5);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abc12fgh", 8);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite within one block (start). */
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_write(file, "xy", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 2);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "xyc12fgh", 8);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite within one block (end). */
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 6);

    rc = nffs_write(file, "<>", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 8);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "xyc12f<>", 8);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite one block middle, extend. */
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 4);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(nffs_getpos(file) == 4);

    rc = nffs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 12);
    TEST_ASSERT(nffs_getpos(file) == 12);
    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "xyc1abcdefgh", 12);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite one block start, extend. */
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 12);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_write(file, "abcdefghijklmnop", 16);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 16);
    rc = nffs_close(file);
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

    struct nffs_file *file;
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite two blocks (middle). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 7);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 7);

    rc = nffs_write(file, "123", 3);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 10);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "abcdefg123klmnop", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks (start). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_write(file, "ABCDEFGHIJ", 10);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 10);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "ABCDEFGHIJklmnop", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks (end). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 6);

    rc = nffs_write(file, "1234567890", 10);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 16);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks middle, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 6);

    rc = nffs_write(file, "1234567890!@#$", 14);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 20);
    TEST_ASSERT(nffs_getpos(file) == 20);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890!@#$", 20);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks start, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 20);
    TEST_ASSERT(nffs_getpos(file) == 20);

    rc = nffs_close(file);
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

    struct nffs_file *file;
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite three blocks (middle). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 6);

    rc = nffs_write(file, "1234567890!@", 12);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 18);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@stuvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks (start). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 20);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "1234567890!@#$%^&*()uvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks (end). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 6);

    rc = nffs_write(file, "1234567890!@#$%^&*", 18);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 24);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks middle, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 6);

    rc = nffs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 26);
    TEST_ASSERT(nffs_getpos(file) == 26);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*()", 26);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks start, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_write(file, "1234567890!@#$%^&*()abcdefghij", 30);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 30);
    TEST_ASSERT(nffs_getpos(file) == 30);

    rc = nffs_close(file);
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

    struct nffs_file *file;
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite middle of first block. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 3);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 3);

    rc = nffs_write(file, "12", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 5);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abc12fghijklmnopqrstuvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite end of first block, start of second. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 0);

    rc = nffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 6);

    rc = nffs_write(file, "1234", 4);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(nffs_getpos(file) == 10);

    rc = nffs_close(file);
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

    rc = nffs_mkdir("/longdir12345678901234567890");
    TEST_ASSERT(rc == 0);

    rc = nffs_rename("/12345678901234567890.txt",
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
    rc = nffs_mkdir("/dir");
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
    rc = nffs_mkdir("/mydir");
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

TEST_CASE(nffs_test_incomplete_block)
{
    struct nffs_block block;
    struct nffs_file *file;
    uint32_t flash_offset;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = nffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/mydir/a", "aaaa", 4);
    nffs_test_util_create_file("/mydir/b", "bbbb", 4);
    nffs_test_util_create_file("/mydir/c", "cccc", 4);

    /* Add a second block to the 'b' file. */
    nffs_test_util_append_file("/mydir/b", "1234", 4);

    /* Corrupt the 'b' file; make it look like the second block only got half
     * written.
     */
    rc = nffs_open("/mydir/b", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    rc = nffs_block_from_hash_entry(&block,
                                   file->nf_inode_entry->nie_last_block_entry);
    TEST_ASSERT(rc == 0);

    nffs_flash_loc_expand(block.nb_hash_entry->nhe_flash_loc, &area_idx,
                         &area_offset);
    flash_offset = nffs_areas[area_idx].na_offset + area_offset;
    rc = flash_native_memset(
            flash_offset + sizeof (struct nffs_disk_block) + 2, 0xff, 2);
    TEST_ASSERT(rc == 0);

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
                }, {
                    .filename = "b",
                    .contents = "bbbb",
                    .contents_len = 4,
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
    struct nffs_file *file;
    uint32_t flash_offset;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = nffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/mydir/a", "aaaa", 4);
    nffs_test_util_create_file("/mydir/b", "bbbb", 4);
    nffs_test_util_create_file("/mydir/c", "cccc", 4);

    /* Add a second block to the 'b' file. */
    nffs_test_util_append_file("/mydir/b", "1234", 4);

    /* Corrupt the 'b' file; overwrite the second block's magic number. */
    rc = nffs_open("/mydir/b", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    rc = nffs_block_from_hash_entry(&block,
                                   file->nf_inode_entry->nie_last_block_entry);
    TEST_ASSERT(rc == 0);

    nffs_flash_loc_expand(block.nb_hash_entry->nhe_flash_loc, &area_idx,
                         &area_offset);
    flash_offset = nffs_areas[area_idx].na_offset + area_offset;
    rc = flash_native_memset(flash_offset, 0x43, 4);
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
                }, {
                    .filename = "b",
                    .contents = "bbbb",
                    .contents_len = 4,
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
        rc = nffs_mkdir(filename);
        TEST_ASSERT(rc == 0);

        for (j = 0; j < 5; j++) {
            snprintf(filename, sizeof filename, "/dir0_%d/dir1_%d", i, j);
            rc = nffs_mkdir(filename);
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
        rc = nffs_unlink(filename);
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

    rc = nffs_unlink("/lvl1dir-0000");
    TEST_ASSERT(rc == 0);

    rc = nffs_unlink("/lvl1dir-0004");
    TEST_ASSERT(rc == 0);

    rc = nffs_mkdir("/lvl1dir-0000");
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


    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = nffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);
    rc = nffs_mkdir("/mydir/dir1");
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
    rc = flash_native_memset(flash_offset + 10, 0xff, 1);
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
            }, {
                .filename = NULL,
            } }
    } };

    nffs_test_assert_system(expected_system, nffs_area_descs);
}

TEST_CASE(nffs_test_cache_large_file)
{
    static char data[NFFS_BLOCK_MAX_DATA_SZ_MAX * 5];
    struct nffs_file *file;
    uint8_t b;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/myfile.txt", data, sizeof data);
    nffs_cache_clear();

    /* Opening a file should not cause any blocks to get cached. */
    rc = nffs_open("/myfile.txt", NFFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt", 0, 0);

    /* Cache first block. */
    rc = nffs_seek(file, nffs_block_max_data_sz * 0);
    TEST_ASSERT(rc == 0);
    rc = nffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 0,
                                     nffs_block_max_data_sz * 1);

    /* Cache second block. */
    rc = nffs_seek(file, nffs_block_max_data_sz * 1);
    TEST_ASSERT(rc == 0);
    rc = nffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 0,
                                     nffs_block_max_data_sz * 2);


    /* Cache fourth block; prior cache should get erased. */
    rc = nffs_seek(file, nffs_block_max_data_sz * 3);
    TEST_ASSERT(rc == 0);
    rc = nffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 3,
                                     nffs_block_max_data_sz * 4);

    /* Cache second and third blocks. */
    rc = nffs_seek(file, nffs_block_max_data_sz * 1);
    TEST_ASSERT(rc == 0);
    rc = nffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 1,
                                     nffs_block_max_data_sz * 4);

    /* Cache fifth block. */
    rc = nffs_seek(file, nffs_block_max_data_sz * 4);
    TEST_ASSERT(rc == 0);
    rc = nffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 1,
                                     nffs_block_max_data_sz * 5);

    rc = nffs_close(file);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(nffs_test_readdir)
{
    struct nffs_dirent *dirent;
    struct nffs_dir *dir;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_area_descs);
    TEST_ASSERT_FATAL(rc == 0);

    rc = nffs_mkdir("/mydir");
    TEST_ASSERT_FATAL(rc == 0);

    nffs_test_util_create_file("/mydir/b", "bbbb", 4);
    nffs_test_util_create_file("/mydir/a", "aaaa", 4);
    rc = nffs_mkdir("/mydir/c");
    TEST_ASSERT_FATAL(rc == 0);

    /* Nonexistent directory. */
    rc = nffs_opendir("/asdf", &dir);
    TEST_ASSERT(rc == NFFS_ENOENT);

    /* Fail to opendir a file. */
    rc = nffs_opendir("/mydir/a", &dir);
    TEST_ASSERT(rc == NFFS_EINVAL);

    /* Real directory (with trailing slash). */
    rc = nffs_opendir("/mydir/", &dir);
    TEST_ASSERT_FATAL(rc == 0);

    rc = nffs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "a");
    TEST_ASSERT(nffs_dirent_is_dir(dirent) == 0);

    rc = nffs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "b");
    TEST_ASSERT(nffs_dirent_is_dir(dirent) == 0);

    rc = nffs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "c");
    TEST_ASSERT(nffs_dirent_is_dir(dirent) == 1);

    rc = nffs_readdir(dir, &dirent);
    TEST_ASSERT(rc == NFFS_ENOENT);

    rc = nffs_closedir(dir);
    TEST_ASSERT(rc == 0);

    /* Root directory. */
    rc = nffs_opendir("/", &dir);
    TEST_ASSERT(rc == 0);
    rc = nffs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_ent_name(dirent, "lost+found");
    TEST_ASSERT(nffs_dirent_is_dir(dirent) == 1);

    rc = nffs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "mydir");
    TEST_ASSERT(nffs_dirent_is_dir(dirent) == 1);

    rc = nffs_closedir(dir);
    TEST_ASSERT(rc == 0);

    /* Delete entries while iterating. */
    rc = nffs_opendir("/mydir", &dir);
    TEST_ASSERT_FATAL(rc == 0);

    rc = nffs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_ent_name(dirent, "a");
    TEST_ASSERT(nffs_dirent_is_dir(dirent) == 0);

    rc = nffs_unlink("/mydir/b");
    TEST_ASSERT(rc == 0);

    rc = nffs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);

    rc = nffs_unlink("/mydir/c");
    TEST_ASSERT(rc == 0);

    rc = nffs_unlink("/mydir");
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_ent_name(dirent, "c");
    TEST_ASSERT(nffs_dirent_is_dir(dirent) == 1);

    rc = nffs_readdir(dir, &dirent);
    TEST_ASSERT(rc == NFFS_ENOENT);

    rc = nffs_closedir(dir);
    TEST_ASSERT(rc == 0);

    /* Ensure directory is gone. */
    rc = nffs_opendir("/mydir", &dir);
    TEST_ASSERT(rc == NFFS_ENOENT);
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
    gen_1_1();
    gen_4_32();
    gen_32_1024();
    nffs_suite_cache();

    return tu_any_failed;
}

#ifdef PKG_TEST

int
main(void)
{
    tu_config.tc_print_results = 1;
    tu_init();

    nffs_test_all();

    return tu_any_failed;
}

#endif
