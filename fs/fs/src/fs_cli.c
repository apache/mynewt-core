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

#include "os/mynewt.h"

#if MYNEWT_VAL(FS_CLI)

#include <inttypes.h>
#include <string.h>

#include <shell/shell.h>
#include <console/console.h>

#include "fs/fs.h"

static int fs_ls_cmd(int argc, char **argv);
static int fs_rm_cmd(int argc, char **argv);
static int fs_mkdir_cmd(int argc, char **argv);
static int fs_mv_cmd(int argc, char **argv);
static int fs_cat_cmd(int argc, char **argv);

static struct shell_cmd fs_ls_struct = {
    .sc_cmd = "ls",
    .sc_cmd_func = fs_ls_cmd
};
static struct shell_cmd fs_rm_struct = {
    .sc_cmd = "rm",
    .sc_cmd_func = fs_rm_cmd
};
static struct shell_cmd fs_mkdir_struct = {
    .sc_cmd = "mkdir",
    .sc_cmd_func = fs_mkdir_cmd
};
static struct shell_cmd fs_mv_struct = {
    .sc_cmd = "mv",
    .sc_cmd_func = fs_mv_cmd
};
static struct shell_cmd fs_cat_struct = {
    .sc_cmd = "cat",
    .sc_cmd_func = fs_cat_cmd
};

static void
fs_ls_file(const char *name, struct fs_file *file)
{
    uint32_t len;

    len = 0;
    fs_filelen(file, &len);
    console_printf("\t%6lu %s\n", (unsigned long)len, name);
}

static void
fs_ls_dir(const char *name)
{
    console_printf("\t%6s %s\n", "dir", name);
}

static int
fs_ls_cmd(int argc, char **argv)
{
    int rc, file_cnt = 0;
    char *path;
    struct fs_file *file;
    struct fs_dir *dir;
    struct fs_dirent *dirent;
    char name[64];
    int plen;
    uint8_t namelen;

    switch (argc) {
    case 1:
        path = "/";
        break;
    case 2:
        path = argv[1];
        break;
    default:
        console_printf("ls <path>\n");
        return 1;
    }

    rc = fs_open(path, FS_ACCESS_READ, &file);
    if (rc == 0) {
        fs_ls_file(path, file);
        fs_close(file);
        file_cnt = 1;
        goto done;
    }

    plen = strlen(path);
    strncpy(name, path, sizeof(name) - 2);
    if (name[plen - 1] != '/') {
        name[plen++] = '/';
        name[plen] = '\0';
    }

    rc = fs_opendir(path, &dir);
    if (rc == 0) {
        do {
            rc = fs_readdir(dir, &dirent);
            if (rc) {
                break;
            }
            if (fs_dirent_name(dirent, sizeof(name) - plen, &name[plen],
                &namelen)) {
                break;
            }
            rc = fs_open(name, FS_ACCESS_READ, &file);
            if (rc == 0) {
                fs_ls_file(name, file);
                fs_close(file);
            } else {
                fs_ls_dir(name);
            }
            file_cnt++;
        } while (1);
        fs_closedir(dir);
        goto done;
    }
    console_printf("Error listing %s - %d\n", path, rc);
done:
    console_printf("%d files\n", file_cnt);
    return 0;
}

static int
fs_rm_cmd(int argc, char **argv)
{
    int i;
    int rc;

    for (i = 1; i < argc; i++) {
        rc = fs_unlink(argv[i]);
        if (rc) {
            console_printf("Error removing %s - %d\n", argv[i], rc);
        }
    }
    return 0;
}

static int
fs_mkdir_cmd(int argc, char **argv)
{
    int i;
    int rc;

    for (i = 1; i < argc; i++) {
        rc = fs_mkdir(argv[1]);
        if (rc) {
            console_printf("Error creating %s - %d\n", argv[i], rc);
        }
    }
    return 0;
}

static int
fs_mv_cmd(int argc, char **argv)
{
    int rc;

    if (argc != 3) {
        rc = -1;
        goto out;
    }
    rc = fs_rename(argv[1], argv[2]);
out:
    if (rc) {
        console_printf("Error moving - %d\n", rc);
    }
    return 0;
}

static int
fs_cat_cmd(int argc, char **argv)
{
    int rc;
    struct fs_file *file;
    char buf[32];
    uint32_t len;

    if (argc != 2) {
        console_printf("cat <filename>\n");
        return -1;
    }

    rc = fs_open(argv[1], FS_ACCESS_READ, &file);
    if (rc != FS_EOK) {
        console_printf("Error opening %s - %d\n", argv[1], rc);
        return -1;
    }

    do {
        rc = fs_read(file, sizeof(buf), buf, &len);
        if (rc != FS_EOK) {
            console_printf("\nError reading %s - %d\n", argv[1], rc);
            break;
        }
        console_write(buf, len);
    } while (len > 0);

    fs_close(file);

    return 0;
}

void
fs_cli_init(void)
{
    shell_cmd_register(&fs_ls_struct);
    shell_cmd_register(&fs_rm_struct);
    shell_cmd_register(&fs_mkdir_struct);
    shell_cmd_register(&fs_mv_struct);
    shell_cmd_register(&fs_cat_struct);
}
#endif /* MYNEWT_VAL(FS_CLI) */
