#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "ffs/ffs.h"
#include "../src/ffs_priv.h"
#include "ffs_test.h"

static const struct ffs_sector_desc ffs_sector_descs[] = {
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
ffs_test_util_assert_len_consistent(const struct ffs_file *file)
{
    const struct ffs_block *block;
    uint32_t data_len;

    data_len = 0;
    SLIST_FOREACH(block, &file->ff_inode->fi_block_list, fb_next) {
        data_len += block->fb_data_len;
    }

    assert(data_len == file->ff_inode->fi_data_len);
}

static void
ffs_test_util_assert_contents(const char *filename, const char *contents,
                              int contents_len)
{
    struct ffs_file *file;
    uint32_t len;
    void *buf;
    int rc;

    rc = ffs_open(&file, filename, FFS_ACCESS_READ);
    assert(rc == 0);

    len = contents_len + 1;
    buf = malloc(len);
    assert(buf != NULL);

    rc = ffs_read(file, buf, &len);
    assert(rc == 0);
    assert(len == contents_len);
    assert(memcmp(buf, contents, contents_len) == 0);
    ffs_test_util_assert_len_consistent(file);

    rc = ffs_close(file);
    assert(rc == 0);

    free(buf);
}

static int
ffs_test_util_block_count(const char *filename)
{
    const struct ffs_block *block;
    struct ffs_file *file;
    int count;
    int rc;

    rc = ffs_open(&file, filename, FFS_ACCESS_READ);
    assert(rc == 0);

    count = 0;
    SLIST_FOREACH(block, &file->ff_inode->fi_block_list, fb_next) {
        count++;
    }

    rc = ffs_close(file);
    assert(rc == 0);

    return count;
}

struct ffs_test_block_desc {
    const char *data;
    int data_len;
};

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

    rc = ffs_open(&file, filename, FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE);
    assert(rc == 0);

    total_len = 0;
    if (num_blocks <= 0) {
        num_writes = 1;
    } else {
        num_writes = num_blocks;
    }
    for (i = 0; i < num_writes; i++) {
        rc = ffs_write(file, blocks[i].data, blocks[i].data_len);
        assert(rc == 0);

        total_len += blocks[i].data_len;
    }

    rc = ffs_close(file);
    assert(rc == 0);

    buf = malloc(total_len);
    assert(buf != NULL);

    offset = 0;
    for (i = 0; i < num_writes; i++) {
        memcpy(buf + offset, blocks[i].data, blocks[i].data_len);
        offset += blocks[i].data_len;
    }
    assert(offset == total_len);

    ffs_test_util_assert_contents(filename, buf, total_len);
    if (num_blocks > 0) {
        assert(ffs_test_util_block_count(filename) == num_blocks);
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

    rc = ffs_open(&file, filename, FFS_ACCESS_WRITE | FFS_ACCESS_APPEND);
    assert(rc == 0);

    rc = ffs_write(file, contents, contents_len);
    assert(rc == 0);

    ffs_test_util_assert_len_consistent(file);

    rc = ffs_close(file);
    assert(rc == 0);
}

struct ffs_test_file_desc {
    const char *filename;
    int is_dir;
    const char *contents;
    int contents_len;
    struct ffs_test_file_desc *children;
};

static void
ffs_test_assert_file(const struct ffs_test_file_desc *file,
                     struct ffs_inode *inode,
                     const char *path)
{
    const struct ffs_test_file_desc *child_file;
    struct ffs_inode *child_inode;
    char *child_path;
    int child_filename_len;
    int path_len;
    int rc;

    inode->fi_flags |= FFS_INODE_F_TEST;

    path_len = strlen(path);

    if (file->is_dir) {
        assert(inode->fi_flags & FFS_INODE_F_DIRECTORY);

        for (child_file = file->children;
             child_file != NULL && child_file->filename != NULL;
             child_file++) {

            child_filename_len = strlen(child_file->filename);
            child_path = malloc(path_len + 1 + child_filename_len);
            assert(child_path != NULL);
            memcpy(child_path, path, path_len);
            child_path[path_len] = '/';
            memcpy(child_path + path_len + 1, child_file->filename,
                   child_filename_len);
            child_path[path_len + 1 + child_filename_len] = '\0';

            rc = ffs_path_find_inode(&child_inode, child_path);
            assert(rc == 0);

            ffs_test_assert_file(child_file, child_inode, child_path);

            free(child_path);
        }
    } else {
        ffs_test_util_assert_contents(path, file->contents,
                                      file->contents_len);
    }
}

static void
ffs_test_assert_branch_touched(struct ffs_inode *inode)
{
    struct ffs_inode *child;

    assert(inode->fi_flags & FFS_INODE_F_TEST);
    inode->fi_flags &= ~FFS_INODE_F_TEST;

    if (inode->fi_flags & FFS_INODE_F_DIRECTORY) {
        SLIST_FOREACH(child, &inode->fi_child_list, fi_sibling_next) {
            ffs_test_assert_branch_touched(child);
        }
    }
}

static void
ffs_test_assert_child_inode_present(const struct ffs_inode *child)
{
    const struct ffs_inode *parent;
    const struct ffs_inode *inode;

    parent = child->fi_parent;
    assert(parent != NULL);

    SLIST_FOREACH(inode, &parent->fi_child_list, fi_sibling_next) {
        if (inode == child) {
            return;
        }
    }

    assert(0);
}

static void
ffs_test_assert_block_present(const struct ffs_block *block)
{
    const struct ffs_inode *inode;
    const struct ffs_block *cur;

    inode = block->fb_inode;
    assert(inode != NULL);

    SLIST_FOREACH(cur, &inode->fi_block_list, fb_next) {
        if (cur == block) {
            return;
        }
    }

    assert(0);
}

static void
ffs_test_assert_children_sorted(const struct ffs_inode *inode)
{
    const struct ffs_inode *child;
    const struct ffs_inode *prev;
    int cmp;
    int rc;

    prev = NULL;
    SLIST_FOREACH(child, &inode->fi_child_list, fi_sibling_next) {
        if (prev != NULL) {
            rc = ffs_inode_filename_cmp_flash(&cmp, prev, child);
            assert(rc == 0);
            assert(cmp < 0);
        }

        if (child->fi_flags & FFS_INODE_F_DIRECTORY) {
            ffs_test_assert_children_sorted(child);
        }

        prev = child;
    }
}

static void
ffs_test_assert_system_once(const struct ffs_test_file_desc *root_dir)
{
    const struct ffs_inode *inode;
    const struct ffs_block *block;
    const struct ffs_base *base;
    int i;

    ffs_test_assert_file(root_dir, ffs_root_dir, "");
    ffs_test_assert_branch_touched(ffs_root_dir);

    /* Ensure no orphaned inodes or blocks. */
    FFS_HASH_FOREACH(base, i) {
        switch (base->fb_type) {
        case FFS_OBJECT_TYPE_INODE:
            inode = (void *)base;
            assert(!(inode->fi_flags &
                        (FFS_INODE_F_DELETED | FFS_INODE_F_DUMMY)));
            if (inode->fi_parent == NULL) {
                assert(inode == ffs_root_dir);
            } else {
                ffs_test_assert_child_inode_present(inode);
            }
            if (!(inode->fi_flags & FFS_INODE_F_DIRECTORY)) {
                assert(inode->fi_data_len ==
                       ffs_inode_calc_data_length(inode));
            }
            break;

        case FFS_OBJECT_TYPE_BLOCK:
            block = (void *)base;
            ffs_test_assert_block_present(block);
            break;

        default:
            assert(0);
            break;
        }
    }

    /* Ensure proper sorting. */
    ffs_test_assert_children_sorted(ffs_root_dir);
}

static void
ffs_test_assert_system(const struct ffs_test_file_desc *root_dir)
{
    int rc;

    /* Ensure files are as specified, and that there are no other files or
     * orphaned inodes / blocks.
     */
    ffs_test_assert_system_once(root_dir);

    /* Force a garbage collection cycle. */
    rc = ffs_gc(NULL);
    assert(rc == 0);

    /* Ensure file system is still as expected. */
    ffs_test_assert_system_once(root_dir);

    /* Clear cached data and restore from flash (i.e, simulate a reboot). */
    rc = ffs_init();
    assert(rc == 0);
    rc = ffs_detect(ffs_sector_descs);
    assert(rc == 0);

    /* Ensure file system is still as expected. */
    ffs_test_assert_system_once(root_dir);
}

static void
ffs_test_assert_sector_seqs(int seq1, int count1, int seq2, int count2)
{
    struct ffs_disk_sector disk_sector;
    int cur1;
    int cur2;
    int rc;
    int i;

    cur1 = 0;
    cur2 = 0;

    for (i = 0; i < ffs_num_sectors; i++) {
        rc = ffs_flash_read(i, 0, &disk_sector, sizeof disk_sector);
        assert(rc == 0);
        assert(ffs_sector_magic_is_set(&disk_sector));
        assert(disk_sector.fds_seq == ffs_sectors[i].fs_seq);
        if (i == ffs_scratch_sector_id) {
            assert(disk_sector.fds_is_scratch == 0xff);
        } else {
            assert(disk_sector.fds_is_scratch == 0);
        }

        if (ffs_sectors[i].fs_seq == seq1) {
            cur1++;
        } else if (ffs_sectors[i].fs_seq == seq2) {
            cur2++;
        } else {
            assert(0);
        }
    }

    assert(cur1 == count1 && cur2 == count2);
}


static void
ffs_test_mkdir(void)
{
    struct ffs_file *file;
    int rc;

    printf("\tmkdir test\n");

    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    rc = ffs_mkdir("/a/b/c/d");
    assert(rc == FFS_ENOENT);

    rc = ffs_mkdir("asdf");
    assert(rc == FFS_EINVAL);

    rc = ffs_mkdir("/a");
    assert(rc == 0);

    rc = ffs_mkdir("/a/b");
    assert(rc == 0);

    rc = ffs_mkdir("/a/b/c");
    assert(rc == 0);

    rc = ffs_mkdir("/a/b/c/d");
    assert(rc == 0);

    rc = ffs_open(&file, "/a/b/c/d/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);

    rc = ffs_close(file);
    assert(rc == 0);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_unlink(void)
{
    const char *filename = "/mytest.txt";
    const char *contents = "unlink test";
    struct ffs_file *file1;
    struct ffs_file *file2;
    uint8_t buf[64];
    uint32_t len;
    int rc;

    printf("\tunlink test\n");

    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    rc = ffs_open(&file1, filename, FFS_ACCESS_READ | FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(file1->ff_inode->fi_refcnt == 2);

    rc = ffs_unlink(filename);
    assert(rc == 0);
    assert(file1->ff_inode->fi_refcnt == 1);

    rc = ffs_open(&file2, filename, FFS_ACCESS_READ);
    assert(rc == FFS_ENOENT);

    rc = ffs_write(file1, contents, strlen(contents));
    assert(rc == 0);

    rc = ffs_seek(file1, 0);
    assert(rc == 0);

    len = sizeof buf;
    rc = ffs_read(file1, buf, &len);
    assert(rc == 0);
    assert(len == strlen(contents));
    assert(memcmp(buf, contents, strlen(contents)) == 0);

    rc = ffs_close(file1);
    assert(rc == 0);

    rc = ffs_open(&file1, filename, FFS_ACCESS_READ);
    assert(rc == FFS_ENOENT);

    struct ffs_test_file_desc *expected_system =
        (struct ffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
    } };

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_rename(void)
{
    struct ffs_file *file;
    const char contents[] = "contents";
    int rc;

    printf("\trename test\n");

    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    rc = ffs_rename("/nonexistent.txt", "/newname.txt");
    assert(rc == FFS_ENOENT);

    /*** Rename file. */
    ffs_test_util_create_file("/myfile.txt", contents, sizeof contents);

    rc = ffs_rename("/myfile.txt", "badname");
    assert(rc == FFS_EINVAL);

    rc = ffs_rename("/myfile.txt", "/myfile2.txt");
    assert(rc == 0);

    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_READ);
    assert(rc == FFS_ENOENT);

    ffs_test_util_assert_contents("/myfile2.txt", contents, sizeof contents);

    rc = ffs_mkdir("/mydir");
    assert(rc == 0);

    rc = ffs_rename("/myfile2.txt", "/mydir/myfile2.txt");
    assert(rc == 0);

    ffs_test_util_assert_contents("/mydir/myfile2.txt", contents,
                                  sizeof contents);

    /*** Rename directory. */
    rc = ffs_rename("/mydir", "badname");
    assert(rc == FFS_EINVAL);

    rc = ffs_rename("/mydir", "/mydir2");
    assert(rc == 0);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_truncate(void)
{
    struct ffs_file *file;
    int rc;

    printf("\ttruncate test\n");

    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    rc = ffs_open(&file, "/myfile.txt",
                  FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 0);
    assert(ffs_getpos(file) == 0);

    rc = ffs_write(file, "abcdefgh", 8);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 8);
    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "abcdefgh", 8);

    rc = ffs_open(&file, "/myfile.txt",
                  FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 0);
    assert(ffs_getpos(file) == 0);

    rc = ffs_write(file, "1234", 4);
    assert(rc == 0);
    assert(ffs_file_len(file) == 4);
    assert(ffs_getpos(file) == 4);
    rc = ffs_close(file);
    assert(rc == 0);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_append(void)
{
    struct ffs_file *file;
    int rc;

    printf("\tappend test\n");

    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE | FFS_ACCESS_APPEND);
    assert(rc == 0);
    assert(ffs_file_len(file) == 0);
    assert(ffs_getpos(file) == 0);

    rc = ffs_write(file, "abcdefgh", 8);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 8);
    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "abcdefgh", 8);

    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE | FFS_ACCESS_APPEND);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 8);

    /* File position should always be at the end of a file after an append.
     * Seek to the middle prior to writing to test this.
     */
    rc = ffs_seek(file, 2);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 2);

    rc = ffs_write(file, "ijklmnop", 8);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 16);
    rc = ffs_write(file, "qrstuvwx", 8);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 24);
    rc = ffs_close(file);
    assert(rc == 0);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_read(void)
{
    struct ffs_file *file;
    uint8_t buf[16];
    uint32_t len;
    int rc;

    printf("\tread test\n");

    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    ffs_test_util_create_file("/myfile.txt", "1234567890", 10);

    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_READ);
    assert(rc == 0);
    assert(ffs_file_len(file) == 10);
    assert(ffs_getpos(file) == 0);

    len = 4;
    rc = ffs_read(file, buf, &len);
    assert(rc == 0);
    assert(len == 4);
    assert(memcmp(buf, "1234", 4) == 0);
    assert(ffs_getpos(file) == 4);

    len = sizeof buf - 4;
    rc = ffs_read(file, buf + 4, &len);
    assert(rc == 0);
    assert(len == 6);
    assert(memcmp(buf, "1234567890", 10) == 0);
    assert(ffs_getpos(file) == 10);

    rc = ffs_close(file);
    assert(rc == 0);
}

static void
ffs_test_overwrite_one(void)
{
    struct ffs_file *file;
    int rc;

    printf("\toverwrite one test\n");

    /*** Setup. */
    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    ffs_test_util_append_file("/myfile.txt", "abcdefgh", 8);

    /*** Overwrite within one block (middle). */
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 3);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 3);

    rc = ffs_write(file, "12", 2);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 5);
    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "abc12fgh", 8);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite within one block (start). */
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 0);

    rc = ffs_write(file, "xy", 2);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 2);
    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "xyc12fgh", 8);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite within one block (end). */
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 6);

    rc = ffs_write(file, "<>", 2);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 8);
    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "xyc12f<>", 8);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite one block middle, extend. */
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 4);
    assert(rc == 0);
    assert(ffs_file_len(file) == 8);
    assert(ffs_getpos(file) == 4);

    rc = ffs_write(file, "abcdefgh", 8);
    assert(rc == 0);
    assert(ffs_file_len(file) == 12);
    assert(ffs_getpos(file) == 12);
    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "xyc1abcdefgh", 12);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite one block start, extend. */
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 12);
    assert(ffs_getpos(file) == 0);

    rc = ffs_write(file, "abcdefghijklmnop", 16);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 16);
    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents("/myfile.txt", "abcdefghijklmnop", 16);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_overwrite_two(void)
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

    printf("\toverwrite two test\n");

    /*** Setup. */
    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    /*** Overwrite two blocks (middle). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 7);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 7);

    rc = ffs_write(file, "123", 3);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 10);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "abcdefg123klmnop", 16);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite two blocks (start). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 0);

    rc = ffs_write(file, "ABCDEFGHIJ", 10);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 10);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "ABCDEFGHIJklmnop", 16);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite two blocks (end). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890", 10);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 16);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890", 16);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite two blocks middle, extend. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890!@#$", 14);
    assert(rc == 0);
    assert(ffs_file_len(file) == 20);
    assert(ffs_getpos(file) == 20);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890!@#$", 20);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite two blocks start, extend. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 16);
    assert(ffs_getpos(file) == 0);

    rc = ffs_write(file, "1234567890!@#$%^&*()", 20);
    assert(rc == 0);
    assert(ffs_file_len(file) == 20);
    assert(ffs_getpos(file) == 20);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt", "1234567890!@#$%^&*()", 20);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_overwrite_three(void)
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

    printf("\toverwrite three test\n");

    /*** Setup. */
    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    /*** Overwrite three blocks (middle). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890!@", 12);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 18);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@stuvwx", 24);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite three blocks (start). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 0);

    rc = ffs_write(file, "1234567890!@#$%^&*()", 20);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 20);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "1234567890!@#$%^&*()uvwx", 24);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite three blocks (end). */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890!@#$%^&*", 18);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 24);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*", 24);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite three blocks middle, extend. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234567890!@#$%^&*()", 20);
    assert(rc == 0);
    assert(ffs_file_len(file) == 26);
    assert(ffs_getpos(file) == 26);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*()", 26);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

    /*** Overwrite three blocks start, extend. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 0);

    rc = ffs_write(file, "1234567890!@#$%^&*()abcdefghij", 30);
    assert(rc == 0);
    assert(ffs_file_len(file) == 30);
    assert(ffs_getpos(file) == 30);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "1234567890!@#$%^&*()abcdefghij", 30);
    assert(ffs_test_util_block_count("/myfile.txt") == 1);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_overwrite_many(void)
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

    printf("\toverwrite many test\n");

    /*** Setup. */
    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    /*** Overwrite middle of first block. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 3);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 3);

    rc = ffs_write(file, "12", 2);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 5);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abc12fghijklmnopqrstuvwx", 24);
    assert(ffs_test_util_block_count("/myfile.txt") == 3);

    /*** Overwrite end of first block, start of second. */
    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = ffs_open(&file, "/myfile.txt", FFS_ACCESS_WRITE);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 0);

    rc = ffs_seek(file, 6);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 6);

    rc = ffs_write(file, "1234", 4);
    assert(rc == 0);
    assert(ffs_file_len(file) == 24);
    assert(ffs_getpos(file) == 10);

    rc = ffs_close(file);
    assert(rc == 0);

    ffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234klmnopqrstuvwx", 24);
    assert(ffs_test_util_block_count("/myfile.txt") == 2);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_long_filename(void)
{
    int rc;

    printf("\tlong filename test\n");

    /*** Setup. */
    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    ffs_test_util_create_file("/12345678901234567890.txt", "contents", 8);

    rc = ffs_mkdir("/longdir12345678901234567890");
    assert(rc == 0);

    rc = ffs_rename("/12345678901234567890.txt",
                    "/longdir12345678901234567890/12345678901234567890.txt");
    assert(rc == 0);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_large_write(void)
{
    static char data[FFS_BLOCK_MAX_DATA_SZ * 5];
    int rc;
    int i;

    printf("\tlarge write test\n");

    /*** Setup. */
    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

    for (i = 0; i < sizeof data; i++) {
        data[i] = i;
    }

    ffs_test_util_create_file("/myfile.txt", data, sizeof data);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_many_children(void)
{
    int rc;

    printf("\tmany children test\n");

    /*** Setup. */
    rc = ffs_format(ffs_sector_descs);
    assert(rc == 0);

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

    ffs_test_assert_system(expected_system);
}

static void
ffs_test_gc(void)
{
    int rc;

    static const struct ffs_sector_desc sector_descs_two[] = {
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

    printf("\tgarbage collection test\n");

    /*** Setup. */
    rc = ffs_format(sector_descs_two);
    assert(rc == 0);

    ffs_test_util_create_file_blocks("/myfile.txt", blocks, 8);

    ffs_gc(NULL);

    assert(ffs_test_util_block_count("/myfile.txt") == 1);
}

static void
ffs_test_wear_level(void)
{
    int rc;
    int i;
    int j;

    static const struct ffs_sector_desc sector_descs_uniform[] = {
        { 0x00000000, 2 * 1024 },
        { 0x00020000, 2 * 1024 },
        { 0x00040000, 2 * 1024 },
        { 0x00060000, 2 * 1024 },
        { 0x00080000, 2 * 1024 },
        { 0, 0 },
    };

    printf("\twear level test\n");

    /*** Setup. */
    rc = ffs_format(sector_descs_uniform);
    assert(rc == 0);

    /* Ensure sectors rotate properly. */
    for (i = 0; i < 255; i++) {
        for (j = 0; j < ffs_num_sectors; j++) {
            ffs_test_assert_sector_seqs(i, ffs_num_sectors - j, i + 1, j);
            ffs_gc(NULL);
        }
    }

    /* Ensure proper rollover of sequence numbers. */
    for (j = 0; j < ffs_num_sectors; j++) {
        ffs_test_assert_sector_seqs(255, ffs_num_sectors - j, 0, j);
        ffs_gc(NULL);
    }
    for (j = 0; j < ffs_num_sectors; j++) {
        ffs_test_assert_sector_seqs(0, ffs_num_sectors - j, 1, j);
        ffs_gc(NULL);
    }
}

int
ffs_test(void)
{
    int rc;

    printf("flash file system testing\n");

    rc = ffs_init();
    assert(rc == 0);

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

    return 0;
}


