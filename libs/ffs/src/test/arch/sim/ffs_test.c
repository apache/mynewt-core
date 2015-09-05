#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "hal/hal_flash.h"
#include "testutil/testutil.h"
#include "ffs/ffs.h"
#include "ffs/ffs_test.h"
#include "ffs_test_priv.h"
#include "../src/ffs_priv.h"

int flash_native_memset(uint32_t offset, uint8_t c, uint32_t len);

static const struct ffs_area_desc ffs_area_descs[] = {
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
ffs_test_util_assert_file_len(struct ffs_file *file, uint32_t expected)
{
    uint32_t len;
    int rc;

    rc = ffs_file_len(file, &len);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(len == expected);
}

static void
ffs_test_util_assert_cache_is_sane(const char *filename)
{
    struct ffs_cache_inode *cache_inode;
    struct ffs_cache_block *cache_block;
    struct ffs_file *file;
    uint32_t cache_start;
    uint32_t cache_end;
    uint32_t block_end;
    int rc;

    rc = ffs_open(filename, FFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    rc = ffs_cache_inode_ensure(&cache_inode, file->ff_inode_entry);
    TEST_ASSERT(rc == 0);

    ffs_cache_inode_range(cache_inode, &cache_start, &cache_end);

    if (TAILQ_EMPTY(&cache_inode->fci_block_list)) {
        TEST_ASSERT(cache_start == 0 && cache_end == 0);
    } else {
        block_end = 0;  /* Pacify gcc. */
        TAILQ_FOREACH(cache_block, &cache_inode->fci_block_list, fcb_link) {
            if (cache_block == TAILQ_FIRST(&cache_inode->fci_block_list)) {
                TEST_ASSERT(cache_block->fcb_file_offset == cache_start);
            } else {
                /* Ensure no gap between this block and its predecessor. */
                TEST_ASSERT(cache_block->fcb_file_offset == block_end);
            }

            block_end = cache_block->fcb_file_offset +
                        cache_block->fcb_block.fb_data_len;
            if (cache_block == TAILQ_LAST(&cache_inode->fci_block_list,
                                          ffs_cache_block_list)) {

                TEST_ASSERT(block_end == cache_end);
            }
        }
    }

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);
}

static void
ffs_test_util_assert_contents(const char *filename, const char *contents,
                              int contents_len)
{
    struct ffs_file *file;
    uint32_t bytes_read;
    void *buf;
    int rc;

    rc = ffs_open(filename, FFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    buf = malloc(contents_len + 1);
    TEST_ASSERT(buf != NULL);

    rc = ffs_read(file, contents_len + 1, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == contents_len);
    TEST_ASSERT(memcmp(buf, contents, contents_len) == 0);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    free(buf);

    ffs_test_util_assert_cache_is_sane(filename);
}

static int
ffs_test_util_block_count(const char *filename)
{
    struct ffs_hash_entry *entry;
    struct ffs_block block;
    struct ffs_file *file;
    int count;
    int rc;

    rc = ffs_open(filename, FFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    count = 0;
    entry = file->ff_inode_entry->fie_last_block_entry;
    while (entry != NULL) {
        count++;
        rc = ffs_block_from_hash_entry(&block, entry);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(block.fb_prev != entry);
        entry = block.fb_prev;
    }

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    return count;
}

static void
ffs_test_util_assert_block_count(const char *filename, int expected_count)
{
    TEST_ASSERT(ffs_test_util_block_count(filename) == expected_count);
}

static void
ffs_test_util_assert_cache_range(const char *filename,
                                 uint32_t expected_cache_start,
                                 uint32_t expected_cache_end)
{
    struct ffs_cache_inode *cache_inode;
    struct ffs_file *file;
    uint32_t cache_start;
    uint32_t cache_end;
    int rc;

    rc = ffs_open(filename, FFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    rc = ffs_cache_inode_ensure(&cache_inode, file->ff_inode_entry);
    TEST_ASSERT(rc == 0);

    ffs_cache_inode_range(cache_inode, &cache_start, &cache_end);
    TEST_ASSERT(cache_start == expected_cache_start);
    TEST_ASSERT(cache_end == expected_cache_end);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_cache_is_sane(filename);
}

static void
ffs_test_util_create_file_blocks(const char *filename,
                                 const struct ffs_test_block_desc *blocks,
                                 int num_blocks)
{
    struct ffs_file *file;
    uint32_t total_len;
    uint32_t offset;
    char *buf;
    int num_writes;
    int rc;
    int i;

    rc = ffs_open(filename, FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE, &file);
    TEST_ASSERT(rc == 0);

    total_len = 0;
    if (num_blocks <= 0) {
        num_writes = 1;
    } else {
        num_writes = num_blocks;
    }
    for (i = 0; i < num_writes; i++) {
        rc = ffs_write(file, blocks[i].data, blocks[i].data_len);
        TEST_ASSERT(rc == 0);

        total_len += blocks[i].data_len;
    }

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    buf = malloc(total_len);
    TEST_ASSERT(buf != NULL);

    offset = 0;
    for (i = 0; i < num_writes; i++) {
        memcpy(buf + offset, blocks[i].data, blocks[i].data_len);
        offset += blocks[i].data_len;
    }
    TEST_ASSERT(offset == total_len);

    ffs_test_util_assert_contents(filename, buf, total_len);
    if (num_blocks > 0) {
        ffs_test_util_assert_block_count(filename, num_blocks);
    }

    free(buf);
}

static void
ffs_test_util_create_file(const char *filename, const char *contents,
                          int contents_len)
{
    struct ffs_test_block_desc block;

    block.data = contents;
    block.data_len = contents_len;

    ffs_test_util_create_file_blocks(filename, &block, 0);
}

static void
ffs_test_util_append_file(const char *filename, const char *contents,
                          int contents_len)
{
    struct ffs_file *file;
    int rc;

    rc = ffs_open(filename, FFS_ACCESS_WRITE | FFS_ACCESS_APPEND, &file);
    TEST_ASSERT(rc == 0);

    rc = ffs_write(file, contents, contents_len);
    TEST_ASSERT(rc == 0);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);
}

static void
ffs_test_copy_area(const struct ffs_area_desc *from,
                   const struct ffs_area_desc *to)
{
    void *buf;
    int rc;

    TEST_ASSERT(from->fad_length == to->fad_length);

    buf = malloc(from->fad_length);
    TEST_ASSERT(buf != NULL);

    rc = flash_read(from->fad_offset, buf, from->fad_length);
    TEST_ASSERT(rc == 0);

    rc = flash_erase(to->fad_offset, to->fad_length);
    TEST_ASSERT(rc == 0);

    rc = flash_write(to->fad_offset, buf, to->fad_length);
    TEST_ASSERT(rc == 0);

    free(buf);
}

static void
ffs_test_util_create_subtree(const char *parent_path,
                             const struct ffs_test_file_desc *elem)
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
            rc = ffs_mkdir(path);
            TEST_ASSERT(rc == 0);
        }

        if (elem->children != NULL) {
            for (i = 0; elem->children[i].filename != NULL; i++) {
                ffs_test_util_create_subtree(path, elem->children + i);
            }
        }
    } else {
        ffs_test_util_create_file(path, elem->contents, elem->contents_len);
    }

    free(path);
}

static void
ffs_test_util_create_tree(const struct ffs_test_file_desc *root_dir)
{
    ffs_test_util_create_subtree(NULL, root_dir);
}

#define FFS_TEST_TOUCHED_ARR_SZ     (16 * 1024)
static struct ffs_hash_entry
    *ffs_test_touched_entries[FFS_TEST_TOUCHED_ARR_SZ];
static int ffs_test_num_touched_entries;

static void
ffs_test_assert_file(const struct ffs_test_file_desc *file,
                     struct ffs_inode_entry *inode_entry,
                     const char *path)
{
    const struct ffs_test_file_desc *child_file;
    struct ffs_inode inode;
    struct ffs_inode_entry *child_inode_entry;
    char *child_path;
    int child_filename_len;
    int path_len;
    int rc;

    TEST_ASSERT(ffs_test_num_touched_entries < FFS_TEST_TOUCHED_ARR_SZ);
    ffs_test_touched_entries[ffs_test_num_touched_entries] =
        &inode_entry->fie_hash_entry;
    ffs_test_num_touched_entries++;

    path_len = strlen(path);

    rc = ffs_inode_from_entry(&inode, inode_entry);
    TEST_ASSERT(rc == 0);

    if (ffs_hash_id_is_dir(inode_entry->fie_hash_entry.fhe_id)) {
        for (child_file = file->children;
             child_file != NULL && child_file->filename != NULL;
             child_file++) {

            child_filename_len = strlen(child_file->filename);
            child_path = malloc(path_len + 1 + child_filename_len);
            TEST_ASSERT(child_path != NULL);
            memcpy(child_path, path, path_len);
            child_path[path_len] = '/';
            memcpy(child_path + path_len + 1, child_file->filename,
                   child_filename_len);
            child_path[path_len + 1 + child_filename_len] = '\0';

            rc = ffs_path_find_inode_entry(child_path, &child_inode_entry);
            TEST_ASSERT(rc == 0);

            ffs_test_assert_file(child_file, child_inode_entry, child_path);

            free(child_path);
        }
    } else {
        ffs_test_util_assert_contents(path, file->contents,
                                      file->contents_len);
    }
}

static void
ffs_test_assert_branch_touched(struct ffs_inode_entry *inode_entry)
{
    struct ffs_inode_entry *child;
    int i;

    if (inode_entry == ffs_lost_found_dir) {
        return;
    }

    for (i = 0; i < ffs_test_num_touched_entries; i++) {
        if (ffs_test_touched_entries[i] == &inode_entry->fie_hash_entry) {
            break;
        }
    }
    TEST_ASSERT(i < ffs_test_num_touched_entries);
    ffs_test_touched_entries[i] = NULL;

    if (ffs_hash_id_is_dir(inode_entry->fie_hash_entry.fhe_id)) {
        SLIST_FOREACH(child, &inode_entry->fie_child_list, fie_sibling_next) {
            ffs_test_assert_branch_touched(child);
        }
    }
}

static void
ffs_test_assert_child_inode_present(struct ffs_inode_entry *child)
{
    const struct ffs_inode_entry *inode_entry;
    const struct ffs_inode_entry *parent;
    struct ffs_inode inode;
    int rc;

    rc = ffs_inode_from_entry(&inode, child);
    TEST_ASSERT(rc == 0);

    parent = inode.fi_parent;
    TEST_ASSERT(parent != NULL);
    TEST_ASSERT(ffs_hash_id_is_dir(parent->fie_hash_entry.fhe_id));

    SLIST_FOREACH(inode_entry, &parent->fie_child_list, fie_sibling_next) {
        if (inode_entry == child) {
            return;
        }
    }

    TEST_ASSERT(0);
}

static void
ffs_test_assert_block_present(struct ffs_hash_entry *block_entry)
{
    const struct ffs_inode_entry *inode_entry;
    struct ffs_hash_entry *cur;
    struct ffs_block block;
    int rc;

    rc = ffs_block_from_hash_entry(&block, block_entry);
    TEST_ASSERT(rc == 0);

    inode_entry = block.fb_inode_entry;
    TEST_ASSERT(inode_entry != NULL);
    TEST_ASSERT(ffs_hash_id_is_file(inode_entry->fie_hash_entry.fhe_id));

    cur = inode_entry->fie_last_block_entry;
    while (cur != NULL) {
        if (cur == block_entry) {
            return;
        }

        rc = ffs_block_from_hash_entry(&block, cur);
        TEST_ASSERT(rc == 0);
        cur = block.fb_prev;
    }

    TEST_ASSERT(0);
}

static void
ffs_test_assert_children_sorted(struct ffs_inode_entry *inode_entry)
{
    struct ffs_inode_entry *child_entry;
    struct ffs_inode_entry *prev_entry;
    struct ffs_inode child_inode;
    struct ffs_inode prev_inode;
    int cmp;
    int rc;

    prev_entry = NULL;
    SLIST_FOREACH(child_entry, &inode_entry->fie_child_list,
                  fie_sibling_next) {
        rc = ffs_inode_from_entry(&child_inode, child_entry);
        TEST_ASSERT(rc == 0);

        if (prev_entry != NULL) {
            rc = ffs_inode_from_entry(&prev_inode, prev_entry);
            TEST_ASSERT(rc == 0);

            rc = ffs_inode_filename_cmp_flash(&prev_inode, &child_inode, &cmp);
            TEST_ASSERT(rc == 0);
            TEST_ASSERT(cmp < 0);
        }

        if (ffs_hash_id_is_dir(child_entry->fie_hash_entry.fhe_id)) {
            ffs_test_assert_children_sorted(child_entry);
        }

        prev_entry = child_entry;
    }
}

static void
ffs_test_assert_system_once(const struct ffs_test_file_desc *root_dir)
{
    struct ffs_inode_entry *inode_entry;
    struct ffs_hash_entry *entry;
    int i;

    ffs_test_num_touched_entries = 0;
    ffs_test_assert_file(root_dir, ffs_root_dir, "");
    ffs_test_assert_branch_touched(ffs_root_dir);

    /* Ensure no orphaned inodes or blocks. */
    FFS_HASH_FOREACH(entry, i) {
        TEST_ASSERT(entry->fhe_flash_loc != FFS_FLASH_LOC_NONE);
        if (ffs_hash_id_is_inode(entry->fhe_id)) {
            inode_entry = (void *)entry;
            TEST_ASSERT(inode_entry->fie_refcnt == 1);
            if (entry->fhe_id == FFS_ID_ROOT_DIR) {
                TEST_ASSERT(inode_entry == ffs_root_dir);
            } else {
                ffs_test_assert_child_inode_present(inode_entry);
            }
        } else {
            ffs_test_assert_block_present(entry);
        }
    }

    /* Ensure proper sorting. */
    ffs_test_assert_children_sorted(ffs_root_dir);
}

static void
ffs_test_assert_system(const struct ffs_test_file_desc *root_dir,
                       const struct ffs_area_desc *area_descs)
{
    int rc;

    /* Ensure files are as specified, and that there are no other files or
     * orphaned inodes / blocks.
     */
    ffs_test_assert_system_once(root_dir);

    /* Force a garbage collection cycle. */
    rc = ffs_gc(NULL);
    TEST_ASSERT(rc == 0);

    /* Ensure file system is still as expected. */
    ffs_test_assert_system_once(root_dir);

    /* Clear cached data and restore from flash (i.e, simulate a reboot). */
    rc = ffs_misc_reset();
    TEST_ASSERT(rc == 0);
    rc = ffs_detect(area_descs);
    TEST_ASSERT(rc == 0);

    /* Ensure file system is still as expected. */
    ffs_test_assert_system_once(root_dir);
}

static void
ffs_test_assert_area_seqs(int seq1, int count1, int seq2, int count2)
{
    struct ffs_disk_area disk_area;
    int cur1;
    int cur2;
    int rc;
    int i;

    cur1 = 0;
    cur2 = 0;

    for (i = 0; i < ffs_num_areas; i++) {
        rc = ffs_flash_read(i, 0, &disk_area, sizeof disk_area);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(ffs_area_magic_is_set(&disk_area));
        TEST_ASSERT(disk_area.fda_gc_seq == ffs_areas[i].fa_gc_seq);
        if (i == ffs_scratch_area_idx) {
            TEST_ASSERT(disk_area.fda_id == FFS_AREA_ID_NONE);
        }

        if (ffs_areas[i].fa_gc_seq == seq1) {
            cur1++;
        } else if (ffs_areas[i].fa_gc_seq == seq2) {
            cur2++;
        } else {
            TEST_ASSERT(0);
        }
    }

    TEST_ASSERT(cur1 == count1 && cur2 == count2);
}

static void
ffs_test_mkdir(void)
{
    struct ffs_file *file;
    int rc;


    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = ffs_mkdir("/a/b/c/d");
    TEST_ASSERT(rc == FFS_ENOENT);

    rc = ffs_mkdir("asdf");
    TEST_ASSERT(rc == FFS_EINVAL);

    rc = ffs_mkdir("/a");
    TEST_ASSERT(rc == 0);

    rc = ffs_mkdir("/a/b");
    TEST_ASSERT(rc == 0);

    rc = ffs_mkdir("/a/b/c");
    TEST_ASSERT(rc == 0);

    rc = ffs_mkdir("/a/b/c/d");
    TEST_ASSERT(rc == 0);

    rc = ffs_open("/a/b/c/d/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "a",
                .is_dir = 1,
                .children = (struct ffs_test_file_desc[]) { {
                    .filename = "b",
                    .is_dir = 1,
                    .children = (struct ffs_test_file_desc[]) { {
                        .filename = "c",
                        .is_dir = 1,
                        .children = (struct ffs_test_file_desc[]) { {
                            .filename = "d",
                            .is_dir = 1,
                            .children = (struct ffs_test_file_desc[]) { {
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

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_unlink)
{
    struct ffs_file *file0;
    struct ffs_file *file1;
    struct ffs_file *file2;
    uint8_t buf[64];
    uint32_t bytes_read;
    int rc;

    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file("/file0.txt", "0", 1);

    rc = ffs_open("/file0.txt", FFS_ACCESS_READ | FFS_ACCESS_WRITE, &file0);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(file0->ff_inode_entry->fie_refcnt == 2);

    rc = ffs_unlink("/file0.txt");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(file0->ff_inode_entry->fie_refcnt == 1);

    rc = ffs_open("/file0.txt", FFS_ACCESS_READ, &file2);
    TEST_ASSERT(rc == FFS_ENOENT);

    rc = ffs_write(file0, "00", 2);
    TEST_ASSERT(rc == 0);

    rc = ffs_seek(file0, 0);
    TEST_ASSERT(rc == 0);

    rc = ffs_read(file0, sizeof buf, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 2);
    TEST_ASSERT(memcmp(buf, "00", 2) == 0);

    rc = ffs_close(file0);
    TEST_ASSERT(rc == 0);

    rc = ffs_open("/file0.txt", FFS_ACCESS_READ, &file0);
    TEST_ASSERT(rc == FFS_ENOENT);

    /* Nested unlink. */
    rc = ffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);
    ffs_test_util_create_file("/mydir/file1.txt", "1", 2);

    rc = ffs_open("/mydir/file1.txt", FFS_ACCESS_READ | FFS_ACCESS_WRITE,
                  &file1);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(file1->ff_inode_entry->fie_refcnt == 2);

    rc = ffs_unlink("/mydir");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(file1->ff_inode_entry->fie_refcnt == 1);

    rc = ffs_open("/mydir/file1.txt", FFS_ACCESS_READ, &file2);
    TEST_ASSERT(rc == FFS_ENOENT);

    rc = ffs_write(file1, "11", 2);
    TEST_ASSERT(rc == 0);

    rc = ffs_seek(file1, 0);
    TEST_ASSERT(rc == 0);

    rc = ffs_read(file1, sizeof buf, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 2);
    TEST_ASSERT(memcmp(buf, "11", 2) == 0);

    rc = ffs_close(file1);
    TEST_ASSERT(rc == 0);

    rc = ffs_open("/mydir/file1.txt", FFS_ACCESS_READ, &file1);
    TEST_ASSERT(rc == FFS_ENOENT);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
    } };

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_rename)
{
    struct ffs_file *file;
    const char contents[] = "contents";
    int rc;


    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = ffs_rename("/nonexistent.txt", "/newname.txt");
    TEST_ASSERT(rc == FFS_ENOENT);

    /*** Rename file. */
    ffs_test_util_create_file("/myfile.txt", contents, sizeof contents);

    rc = ffs_rename("/myfile.txt", "badname");
    TEST_ASSERT(rc == FFS_EINVAL);

    rc = ffs_rename("/myfile.txt", "/myfile2.txt");
    TEST_ASSERT(rc == 0);

    rc = ffs_open("/myfile.txt", FFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FFS_ENOENT);

    ffs_test_util_assert_contents("/myfile2.txt", contents, sizeof contents);

    rc = ffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    rc = ffs_rename("/myfile2.txt", "/mydir/myfile2.txt");
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/mydir/myfile2.txt", contents,
                                  sizeof contents);

    /*** Rename directory. */
    rc = ffs_rename("/mydir", "badname");
    TEST_ASSERT(rc == FFS_EINVAL);

    rc = ffs_rename("/mydir", "/mydir2");
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/mydir2/myfile2.txt", contents,
                                  sizeof contents);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "mydir2",
                .is_dir = 1,
                .children = (struct ffs_test_file_desc[]) { {
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

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_truncate)
{
    struct ffs_file *file;
    int rc;


    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE,
                  &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 8);
    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "abcdefgh", 8);

    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE,
                  &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_write(file, "1234", 4);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 4);
    TEST_ASSERT(ffs_getpos(file) == 4);
    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "1234", 4);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "1234",
                .contents_len = 4,
            }, {
                .filename = NULL,
            } },
    } };

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_append)
{
    struct ffs_file *file;
    int rc;


    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE | FFS_ACCESS_APPEND, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 8);
    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "abcdefgh", 8);

    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE | FFS_ACCESS_APPEND, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 8);

    /* File position should always be at the end of a file after an append.
     * Seek to the middle prior to writing to test this.
     */
    rc = ffs_seek(file, 2);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 2);

    rc = ffs_write(file, "ijklmnop", 8);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 16);
    rc = ffs_write(file, "qrstuvwx", 8);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 24);
    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt",
                                  "abcdefghijklmnopqrstuvwx", 24);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdefghijklmnopqrstuvwx",
                .contents_len = 24,
            }, {
                .filename = NULL,
            } },
    } };

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_read)
{
    struct ffs_file *file;
    uint8_t buf[16];
    uint32_t bytes_read;
    int rc;

    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file("/myfile.txt", "1234567890", 10);

    rc = ffs_open("/myfile.txt", FFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 10);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_read(file, 4, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 4);
    TEST_ASSERT(memcmp(buf, "1234", 4) == 0);
    TEST_ASSERT(ffs_getpos(file) == 4);

    rc = ffs_read(file, sizeof buf - 4, buf + 4, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 6);
    TEST_ASSERT(memcmp(buf, "1234567890", 10) == 0);
    TEST_ASSERT(ffs_getpos(file) == 10);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(ffs_test_overwrite_one)
{
    struct ffs_file *file;
    int rc;


    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    ffs_test_util_append_file("/myfile.txt", "abcdefgh", 8);

    /*** Overwrite within one block (middle). */
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 3);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 3);

    rc = ffs_write(file, "12", 2);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 5);
    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "abc12fgh", 8);
    ffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite within one block (start). */
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_write(file, "xy", 2);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 2);
    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "xyc12fgh", 8);
    ffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite within one block (end). */
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 6);

    rc = ffs_write(file, "<>", 2);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 8);
    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "xyc12f<>", 8);
    ffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite one block middle, extend. */
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 4);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(ffs_getpos(file) == 4);

    rc = ffs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 12);
    TEST_ASSERT(ffs_getpos(file) == 12);
    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "xyc1abcdefgh", 12);
    ffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite one block start, extend. */
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 12);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_write(file, "abcdefghijklmnop", 16);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 16);
    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "abcdefghijklmnop", 16);
    ffs_test_util_assert_block_count("/myfile.txt", 1);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdefghijklmnop",
                .contents_len = 16,
            }, {
                .filename = NULL,
            } },
    } };

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_overwrite_two)
{
    struct ffs_test_block_desc *blocks = (struct ffs_test_block_desc[]) { {
        .data = "abcdefgh",
        .data_len = 8,
    }, {
        .data = "ijklmnop",
        .data_len = 8,
    } };

    struct ffs_file *file;
    int rc;


    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite two blocks (middle). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 7);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 7);

    rc = ffs_write(file, "123", 3);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 10);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "abcdefg123klmnop", 16);
    ffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks (start). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_write(file, "ABCDEFGHIJ", 10);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 10);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "ABCDEFGHIJklmnop", 16);
    ffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks (end). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890", 10);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 16);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890", 16);
    ffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks middle, extend. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890!@#$", 14);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 20);
    TEST_ASSERT(ffs_getpos(file) == 20);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890!@#$", 20);
    ffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks start, extend. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 20);
    TEST_ASSERT(ffs_getpos(file) == 20);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "1234567890!@#$%^&*()", 20);
    ffs_test_util_assert_block_count("/myfile.txt", 2);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "1234567890!@#$%^&*()",
                .contents_len = 20,
            }, {
                .filename = NULL,
            } },
    } };

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_overwrite_three)
{
    struct ffs_test_block_desc *blocks = (struct ffs_test_block_desc[]) { {
        .data = "abcdefgh",
        .data_len = 8,
    }, {
        .data = "ijklmnop",
        .data_len = 8,
    }, {
        .data = "qrstuvwx",
        .data_len = 8,
    } };

    struct ffs_file *file;
    int rc;


    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite three blocks (middle). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890!@", 12);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 18);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@stuvwx", 24);
    ffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks (start). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 20);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "1234567890!@#$%^&*()uvwx", 24);
    ffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks (end). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890!@#$%^&*", 18);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 24);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*", 24);
    ffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks middle, extend. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 26);
    TEST_ASSERT(ffs_getpos(file) == 26);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*()", 26);
    ffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks start, extend. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_write(file, "1234567890!@#$%^&*()abcdefghij", 30);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 30);
    TEST_ASSERT(ffs_getpos(file) == 30);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "1234567890!@#$%^&*()abcdefghij", 30);
    ffs_test_util_assert_block_count("/myfile.txt", 3);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "1234567890!@#$%^&*()abcdefghij",
                .contents_len = 30,
            }, {
                .filename = NULL,
            } },
    } };

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_overwrite_many)
{
    struct ffs_test_block_desc *blocks = (struct ffs_test_block_desc[]) { {
        .data = "abcdefgh",
        .data_len = 8,
    }, {
        .data = "ijklmnop",
        .data_len = 8,
    }, {
        .data = "qrstuvwx",
        .data_len = 8,
    } };

    struct ffs_file *file;
    int rc;


    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite middle of first block. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 3);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 3);

    rc = ffs_write(file, "12", 2);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 5);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abc12fghijklmnopqrstuvwx", 24);
    ffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite end of first block, start of second. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open("/myfile.txt", FFS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234", 4);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(ffs_getpos(file) == 10);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234klmnopqrstuvwx", 24);
    ffs_test_util_assert_block_count("/myfile.txt", 3);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdef1234klmnopqrstuvwx",
                .contents_len = 24,
            }, {
                .filename = NULL,
            } },
    } };

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_long_filename)
{
    int rc;


    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file("/12345678901234567890.txt", "contents", 8);

    rc = ffs_mkdir("/longdir12345678901234567890");
    TEST_ASSERT(rc == 0);

    rc = ffs_rename("/12345678901234567890.txt",
                    "/longdir12345678901234567890/12345678901234567890.txt");
    TEST_ASSERT(rc == 0);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "longdir12345678901234567890",
                .is_dir = 1,
                .children = (struct ffs_test_file_desc[]) { {
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

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_large_write)
{
    static char data[FFS_BLOCK_MAX_DATA_SZ_MAX * 5];
    int rc;
    int i;

    static const struct ffs_area_desc area_descs_two[] = {
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0, 0 },
    };



    /*** Setup. */
    rc = ffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof data; i++) {
        data[i] = i;
    }

    ffs_test_util_create_file("/myfile.txt", data, sizeof data);

    /* Ensure large write was split across the appropriate number of data
     * blocks.
     */
    TEST_ASSERT(ffs_test_util_block_count("/myfile.txt") ==
           sizeof data / FFS_BLOCK_MAX_DATA_SZ_MAX);

    /* Garbage collect and then ensure the large file is still properly divided
     * according to max data block size.
     */
    ffs_gc(NULL);
    TEST_ASSERT(ffs_test_util_block_count("/myfile.txt") ==
           sizeof data / FFS_BLOCK_MAX_DATA_SZ_MAX);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = data,
                .contents_len = sizeof data,
            }, {
                .filename = NULL,
            } },
    } };

    ffs_test_assert_system(expected_system, area_descs_two);
}

TEST_CASE(ffs_test_many_children)
{
    int rc;


    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file("/zasdf", NULL, 0);
    ffs_test_util_create_file("/FfD", NULL, 0);
    ffs_test_util_create_file("/4Zvv", NULL, 0);
    ffs_test_util_create_file("/*(*2fs", NULL, 0);
    ffs_test_util_create_file("/pzzd", NULL, 0);
    ffs_test_util_create_file("/zasdf0", NULL, 0);
    ffs_test_util_create_file("/23132.bin", NULL, 0);
    ffs_test_util_create_file("/asldkfjaldskfadsfsdf.txt", NULL, 0);
    ffs_test_util_create_file("/sdgaf", NULL, 0);
    ffs_test_util_create_file("/939302**", NULL, 0);
    rc = ffs_mkdir("/dir");
    ffs_test_util_create_file("/dir/itw82", NULL, 0);
    ffs_test_util_create_file("/dir/124", NULL, 0);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) {
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
                    .children = (struct ffs_test_file_desc[]) {
                        { "itw82" },
                        { "124" },
                        { NULL },
                    },
                },
                { NULL },
            }
    } };

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_gc)
{
    int rc;

    static const struct ffs_area_desc area_descs_two[] = {
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0, 0 },
    };

    struct ffs_test_block_desc blocks[8] = { {
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


    rc = ffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 8);

    ffs_gc(NULL);

    ffs_test_util_assert_block_count("/myfile.txt", 1);
}

TEST_CASE(ffs_test_wear_level)
{
    int rc;
    int i;
    int j;

    static const struct ffs_area_desc area_descs_uniform[] = {
        { 0x00000000, 2 * 1024 },
        { 0x00020000, 2 * 1024 },
        { 0x00040000, 2 * 1024 },
        { 0x00060000, 2 * 1024 },
        { 0x00080000, 2 * 1024 },
        { 0, 0 },
    };


    /*** Setup. */
    rc = ffs_format(area_descs_uniform);
    TEST_ASSERT(rc == 0);

    /* Ensure areas rotate properly. */
    for (i = 0; i < 255; i++) {
        for (j = 0; j < ffs_num_areas; j++) {
            ffs_test_assert_area_seqs(i, ffs_num_areas - j, i + 1, j);
            ffs_gc(NULL);
        }
    }

    /* Ensure proper rollover of sequence numbers. */
    for (j = 0; j < ffs_num_areas; j++) {
        ffs_test_assert_area_seqs(255, ffs_num_areas - j, 0, j);
        ffs_gc(NULL);
    }
    for (j = 0; j < ffs_num_areas; j++) {
        ffs_test_assert_area_seqs(0, ffs_num_areas - j, 1, j);
        ffs_gc(NULL);
    }
}

TEST_CASE(ffs_test_corrupt_scratch)
{
    int non_scratch_id;
    int scratch_id;
    int rc;

    static const struct ffs_area_desc area_descs_two[] = {
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0, 0 },
    };


    /*** Setup. */
    rc = ffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file("/myfile.txt", "contents", 8);

    /* Copy the current contents of the non-scratch area to the scratch area.
     * This will make the scratch area look like it only partially participated
     * in a garbage collection cycle.
     */
    scratch_id = ffs_scratch_area_idx;
    non_scratch_id = scratch_id ^ 1;
    ffs_test_copy_area(area_descs_two + non_scratch_id,
                       area_descs_two + ffs_scratch_area_idx);

    /* Add some more data to the non-scratch area. */
    rc = ffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    /* Ensure the file system is successfully detected and valid, despite
     * corruption.
     */

    rc = ffs_misc_reset();
    TEST_ASSERT(rc == 0);

    rc = ffs_detect(area_descs_two);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(ffs_scratch_area_idx == scratch_id);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
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

    ffs_test_assert_system(expected_system, area_descs_two);
}

TEST_CASE(ffs_test_incomplete_block)
{
    struct ffs_block block;
    struct ffs_file *file;
    uint32_t flash_offset;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = ffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file("/mydir/a", "aaaa", 4);
    ffs_test_util_create_file("/mydir/b", "bbbb", 4);
    ffs_test_util_create_file("/mydir/c", "cccc", 4);

    /* Add a second block to the 'b' file. */
    ffs_test_util_append_file("/mydir/b", "1234", 4);

    /* Corrupt the 'b' file; make it look like the second block only got half
     * written.
     */
    rc = ffs_open("/mydir/b", FFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    rc = ffs_block_from_hash_entry(&block,
                                   file->ff_inode_entry->fie_last_block_entry);
    TEST_ASSERT(rc == 0);

    ffs_flash_loc_expand(block.fb_hash_entry->fhe_flash_loc, &area_idx,
                         &area_offset);
    flash_offset = ffs_areas[area_idx].fa_offset + area_offset;
    rc = flash_native_memset(flash_offset + sizeof (struct ffs_disk_block) + 2,
                             0xff, 2);
    TEST_ASSERT(rc == 0);

    rc = ffs_misc_reset();
    TEST_ASSERT(rc == 0);
    rc = ffs_detect(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    /* The entire second block should be removed; the file should only contain
     * the first block.
     */
    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "mydir",
                .is_dir = 1,
                .children = (struct ffs_test_file_desc[]) { {
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

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_corrupt_block)
{
    struct ffs_block block;
    struct ffs_file *file;
    uint32_t flash_offset;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = ffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file("/mydir/a", "aaaa", 4);
    ffs_test_util_create_file("/mydir/b", "bbbb", 4);
    ffs_test_util_create_file("/mydir/c", "cccc", 4);

    /* Add a second block to the 'b' file. */
    ffs_test_util_append_file("/mydir/b", "1234", 4);

    /* Corrupt the 'b' file; overwrite the second block's magic number. */
    rc = ffs_open("/mydir/b", FFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);

    rc = ffs_block_from_hash_entry(&block,
                                   file->ff_inode_entry->fie_last_block_entry);
    TEST_ASSERT(rc == 0);

    ffs_flash_loc_expand(block.fb_hash_entry->fhe_flash_loc, &area_idx,
                         &area_offset);
    flash_offset = ffs_areas[area_idx].fa_offset + area_offset;
    rc = flash_native_memset(flash_offset, 0x43, 4);
    TEST_ASSERT(rc == 0);

    /* Write a fourth file. This file should get restored even though the
     * previous object has an invalid magic number.
     */
    ffs_test_util_create_file("/mydir/d", "dddd", 4);

    rc = ffs_misc_reset();
    TEST_ASSERT(rc == 0);
    rc = ffs_detect(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    /* The entire second block should be removed; the file should only contain
     * the first block.
     */
    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "mydir",
                .is_dir = 1,
                .children = (struct ffs_test_file_desc[]) { {
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

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_large_unlink)
{
    static char file_contents[1024 * 4];
    char filename[256];
    int rc;
    int i;
    int j;
    int k;


    /*** Setup. */
    ffs_config.fc_num_inodes = 1024;
    ffs_config.fc_num_blocks = 1024;

    rc = ffs_init();
    TEST_ASSERT(rc == 0);

    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < 5; i++) {
        snprintf(filename, sizeof filename, "/dir0_%d", i);
        rc = ffs_mkdir(filename);
        TEST_ASSERT(rc == 0);

        for (j = 0; j < 5; j++) {
            snprintf(filename, sizeof filename, "/dir0_%d/dir1_%d", i, j);
            rc = ffs_mkdir(filename);
            TEST_ASSERT(rc == 0);

            for (k = 0; k < 5; k++) {
                snprintf(filename, sizeof filename,
                         "/dir0_%d/dir1_%d/file2_%d", i, j, k);
                ffs_test_util_create_file(filename, file_contents,
                                          sizeof file_contents);
            }
        }

        for (j = 0; j < 15; j++) {
            snprintf(filename, sizeof filename, "/dir0_%d/file1_%d", i, j);
            ffs_test_util_create_file(filename, file_contents,
                                      sizeof file_contents);
        }
    }

    for (i = 0; i < 5; i++) {
        snprintf(filename, sizeof filename, "/dir0_%d", i);
        rc = ffs_unlink(filename);
        TEST_ASSERT(rc == 0);
    }

    /* The entire file system should be empty. */
    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
    } };

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_large_system)
{
    int rc;


    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);
    ffs_test_util_create_tree(ffs_test_system_01);

    ffs_test_assert_system(ffs_test_system_01, ffs_area_descs);

    rc = ffs_unlink("/lvl1dir-0000");
    TEST_ASSERT(rc == 0);

    rc = ffs_unlink("/lvl1dir-0004");
    TEST_ASSERT(rc == 0);

    rc = ffs_mkdir("/lvl1dir-0000");
    TEST_ASSERT(rc == 0);

    ffs_test_assert_system(ffs_test_system_01_rm_1014_mk10, ffs_area_descs);
}

TEST_CASE(ffs_test_lost_found)
{
    char buf[32];
    struct ffs_inode_entry *inode_entry;
    uint32_t flash_offset;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;


    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    rc = ffs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);
    rc = ffs_mkdir("/mydir/dir1");
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file("/mydir/file1", "aaaa", 4);
    ffs_test_util_create_file("/mydir/dir1/file2", "bbbb", 4);

    /* Corrupt the mydir inode. */
    rc = ffs_path_find_inode_entry("/mydir", &inode_entry);
    TEST_ASSERT(rc == 0);

    snprintf(buf, sizeof buf, "%lu",
             (unsigned long)inode_entry->fie_hash_entry.fhe_id);

    ffs_flash_loc_expand(inode_entry->fie_hash_entry.fhe_flash_loc,
                         &area_idx, &area_offset);
    flash_offset = ffs_areas[area_idx].fa_offset + area_offset;
    rc = flash_native_memset(flash_offset + 10, 0xff, 1);
    TEST_ASSERT(rc == 0);

    /* Clear cached data and restore from flash (i.e, simulate a reboot). */
    rc = ffs_misc_reset();
    TEST_ASSERT(rc == 0);
    rc = ffs_detect(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    /* All contents should now be in the lost+found dir. */
    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct ffs_test_file_desc[]) { {
                .filename = "lost+found",
                .is_dir = 1,
                .children = (struct ffs_test_file_desc[]) { {
                    .filename = buf,
                    .is_dir = 1,
                    .children = (struct ffs_test_file_desc[]) { {
                        .filename = "file1",
                        .contents = "aaaa",
                        .contents_len = 4,
                    }, {
                        .filename = "dir1",
                        .is_dir = 1,
                        .children = (struct ffs_test_file_desc[]) { {
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

    ffs_test_assert_system(expected_system, ffs_area_descs);
}

TEST_CASE(ffs_test_cache_large_file)
{
    static char data[FFS_BLOCK_MAX_DATA_SZ_MAX * 5];
    struct ffs_file *file;
    uint8_t b;
    int rc;

    /*** Setup. */
    rc = ffs_format(ffs_area_descs);
    TEST_ASSERT(rc == 0);

    ffs_test_util_create_file("/myfile.txt", data, sizeof data);
    ffs_cache_clear();

    /* Opening a file should not cause any blocks to get cached. */
    rc = ffs_open("/myfile.txt", FFS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_cache_range("/myfile.txt", 0, 0);

    /* Cache first block. */
    rc = ffs_seek(file, ffs_block_max_data_sz * 0);
    TEST_ASSERT(rc == 0);
    rc = ffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_cache_range("/myfile.txt",
                                     ffs_block_max_data_sz * 0,
                                     ffs_block_max_data_sz * 1);

    /* Cache second block. */
    rc = ffs_seek(file, ffs_block_max_data_sz * 1);
    TEST_ASSERT(rc == 0);
    rc = ffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_cache_range("/myfile.txt",
                                     ffs_block_max_data_sz * 0,
                                     ffs_block_max_data_sz * 2);


    /* Cache fourth block; prior cache should get erased. */
    rc = ffs_seek(file, ffs_block_max_data_sz * 3);
    TEST_ASSERT(rc == 0);
    rc = ffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_cache_range("/myfile.txt",
                                     ffs_block_max_data_sz * 3,
                                     ffs_block_max_data_sz * 4);

    /* Cache second and third blocks. */
    rc = ffs_seek(file, ffs_block_max_data_sz * 1);
    TEST_ASSERT(rc == 0);
    rc = ffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_cache_range("/myfile.txt",
                                     ffs_block_max_data_sz * 1,
                                     ffs_block_max_data_sz * 4);

    /* Cache fifth block. */
    rc = ffs_seek(file, ffs_block_max_data_sz * 4);
    TEST_ASSERT(rc == 0);
    rc = ffs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    ffs_test_util_assert_cache_range("/myfile.txt",
                                     ffs_block_max_data_sz * 1,
                                     ffs_block_max_data_sz * 5);

    rc = ffs_close(file);
    TEST_ASSERT(rc == 0);
}

TEST_SUITE(ffs_suite_cache)
{
    int rc;

    memset(&ffs_config, 0, sizeof ffs_config);
    ffs_config.fc_num_cache_inodes = 4;
    ffs_config.fc_num_cache_blocks = 64;

    rc = ffs_init();
    TEST_ASSERT(rc == 0);

    ffs_test_cache_large_file();
}

static void
ffs_test_gen(void)
{
    int rc;

    rc = ffs_init();
    TEST_ASSERT(rc == 0);

    ffs_test_unlink();
    ffs_test_mkdir();
    ffs_test_rename();
    ffs_test_truncate();
    ffs_test_append();
    ffs_test_read();
    ffs_test_overwrite_one();
    ffs_test_overwrite_two();
    ffs_test_overwrite_three();
    ffs_test_overwrite_many();
    ffs_test_long_filename();
    ffs_test_large_write();
    ffs_test_many_children();
    ffs_test_gc();
    ffs_test_wear_level();
    ffs_test_corrupt_scratch();
    ffs_test_incomplete_block();
    ffs_test_corrupt_block();
    ffs_test_large_unlink();
    ffs_test_large_system();
    ffs_test_lost_found();
}

TEST_SUITE(gen_1_1)
{
    ffs_config.fc_num_cache_inodes = 1;
    ffs_config.fc_num_cache_blocks = 1;
    ffs_test_gen();
}

TEST_SUITE(gen_4_32)
{
    ffs_config.fc_num_cache_inodes = 4;
    ffs_config.fc_num_cache_blocks = 32;
    ffs_test_gen();
}

TEST_SUITE(gen_32_1024)
{
    ffs_config.fc_num_cache_inodes = 32;
    ffs_config.fc_num_cache_blocks = 1024;
    ffs_test_gen();
}

int
ffs_test_all(void)
{
    gen_1_1();
    gen_4_32();
    gen_32_1024();
    ffs_suite_cache();

    return tu_any_failed;
}

#ifdef PKG_TEST

int
main(void)
{
    tu_config.tc_base_path = NULL;//".";
    tu_config.tc_verbose = 1;
    tu_init();

    ffs_test_all();

    return tu_any_failed;
}

#endif
