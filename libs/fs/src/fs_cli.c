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

#ifdef SHELL_PRESENT

#include <fs/fs.h>

#include <shell/shell.h>
#include <console/console.h>

static struct shell_cmd fs_ls_struct;

#if 0
static void
fs_ls_file(const char *name, struct fs_file *file)
{
    uint32_t len;

    len = 0;
    fs_filelen(file, &len);
    console_printf("\t%6d %s\n", len, name);
}
#endif

static int
fs_ls_cmd(int argc, char **argv)
{
    int rc;
    char *path;
    struct fs_file *file;
    struct fs_dir *dir;

    switch (argc) {
    case 1:
        path = NULL;
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
#if 0
        fs_ls_file(path, file);
#endif
        fs_close(file);
    }
    console_printf("fs_open() = %d\n", rc);

    rc = fs_opendir(path, &dir);
    console_printf("fs_opendir() = %d\n", rc);
    if (rc == 0) {
        fs_closedir(dir);
    }
    return 0;
}

void
fs_cli_init(void)
{
    int rc;

    rc = shell_cmd_register(&fs_ls_struct, "ls", fs_ls_cmd);
    if (rc != 0) {
        return;
    }
}
#endif /* SHELL_PRESENT */
