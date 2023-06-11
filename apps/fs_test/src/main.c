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
#include <bsp/bsp.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <log/log.h>
#include "flash_map/flash_map.h"
#include <fs/fs.h>
#include <hal/hal_gpio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

#define BLINKY_PRIO          (8)
#define BLINKY_STACK_SIZE    OS_STACK_ALIGN(64)
static struct os_task blinky_task;

#define FS_TEST_PRIO         (9)
#define FS_TEST_STACK_SIZE   OS_STACK_ALIGN(2048)
static struct os_task fs_test_task;

#if !MYNEWT_VAL(FS_TEST_LITTLEFS) && !MYNEWT_VAL(FS_TEST_NFFS)
#error "No filesystem selected, or unsupported FS!"
#endif

static char *random_strings[] = {
    "Q4qrwYFQIzCj8JsjxIVQIywAWkkFo2kk",
    "sEIdSP7uG6XkJr3ZkOCYPL8Rj80gGPVe2w",
    "idZNVRMBuaYP3E8CSL36NXYpGPj5ED",
    "000o2PHKjvxfV4AuvDaqye2QPJK7269",
    "R3Xg4daYGr",
};

/* Root directory where files will be created */
static const char *dirformat = "fs_test_%d";

#define STARTUP_DELAY MYNEWT_VAL(FS_TEST_STARTUP_DELAY)
#define MAX_TEST_FILES MYNEWT_VAL(FS_TEST_MAX_FILES)

#define BLINK_NORMAL (OS_TICKS_PER_SEC)
#define BLINK_SLOW (OS_TICKS_PER_SEC * 2)
#define BLINK_FAST (OS_TICKS_PER_SEC / 2)
static uint32_t g_blink_freq = BLINK_NORMAL;

static int
fs_test_create_directory(char *dirname)
{
    char name[20];
    int i;
    int rc;

    i = 0;
    while (true) {
        sprintf(name, dirformat, i);
        rc = fs_mkdir(name);
        if (rc != FS_EEXIST) {
            break;
        }
        i++;
    }

    if (rc == FS_EOK) {
        printf("Created new test directory (%s)\n", name);
        strcpy(dirname, name);
    } else {
        printf("Failed creating test directory (%d)\n", rc);
    }

    return rc;
}

static int
fs_test_write_files(char *root)
{
    int i;
    char name[30];
    struct fs_file *file = NULL;
    char *string;
    uint32_t len;
    int rc;

    for (i = 0; i < MAX_TEST_FILES; i++) {
        sprintf(name, "%s/test_%d", root, i);
        string = random_strings[i % ARRAY_SIZE(random_strings)];
        len = strlen(string);

        printf("Opening new file (%s) for writing... ", name);
        rc = fs_open(name, FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE, &file);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            return -1;
        }
        printf("ok\n");

        printf("Writing data to new file... ");
        rc = fs_write(file, string, len);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            fs_close(file);
            return -1;
        }
        printf("ok\n");

        fs_close(file);
    }

    return 0;
}

static int
fs_test_read_files(char *root)
{
    int i;
    char name[30];
    char buf[45];
    struct fs_file *file = NULL;
    char *string;
    uint32_t len;
    uint32_t outlen;
    uint32_t pos;
    int rc;

    for (i = 0; i < MAX_TEST_FILES; i++) {
        sprintf(name, "%s/test_%d", root, i);
        string = random_strings[i % ARRAY_SIZE(random_strings)];
        len = strlen(string);

        printf("Opening new file (%s) for reading... ", name);
        rc = fs_open(name, FS_ACCESS_READ, &file);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            printf("Failed opening for reading: %d\n", rc);
            return rc;
        }
        printf("ok\n");

        printf("Getting file length... ");
        rc = fs_filelen(file, &outlen);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            fs_close(file);
            return rc;
        }
        printf("ok (%ld)\n", outlen);

        if (len != outlen) {
            printf("%s has an unexpected length (%lu!=%lu)\n", name, len, outlen);
            fs_close(file);
            return -1;
        }

        pos = fs_getpos(file);
        if (pos != 0) {
            printf("Invalid position (%lu), should be (0)\n", pos);
            fs_close(file);
            return -1;
        }

        printf("Reading from file... ");
        memset(buf, 0, sizeof(buf));
        rc = fs_read(file, len, buf, &outlen);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            fs_close(file);
            return rc;
        }
        printf("ok\n");

        printf("Compare read results... ");
        if (strncmp(buf, string, len) != 0) {
            printf("fail\n");
            fs_close(file);
            return -1;
        }
        printf("ok\n");

        pos = fs_getpos(file);
        if (pos != len) {
            printf("Invalid position (%lu), should be (%lu)\n", pos, len);
            fs_close(file);
            return -1;
        }

        printf("Seek to middle position of file... ");
        rc = fs_seek(file, len / 2);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            fs_close(file);
            return rc;
        }
        printf("ok\n");

        pos = fs_getpos(file);
        if (pos != len / 2) {
            printf("Invalid position (%lu), should be (%lu)\n", pos, len / 2);
            fs_close(file);
            return -1;
        }

        printf("Reading again... ");
        memset(buf, 0, sizeof(buf));
        rc = fs_read(file, len / 2, buf, &outlen);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            fs_close(file);
            return -1;
        }
        printf("ok\n");

        printf("Comparing read results... ");
        if (strncmp(buf, &string[len / 2], len / 2) != 0) {
            printf("fail\n");
            fs_close(file);
            return -1;
        }
        printf("ok\n");

        fs_close(file);
    }

    return 0;
}

static int
fs_test_rename_files(char *root)
{
    int i;
    char name[30];
    char new_name[30];
    int rc;

    for (i = 0; i < MAX_TEST_FILES; i++) {
        sprintf(name, "%s/test_%d", root, i);
        sprintf(new_name, "%s/tested_%d", root, i);

        printf("Renaming (%s) to (%s)... ", name, new_name);
        rc = fs_rename(name, new_name);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            return -1;
        }
        printf("ok\n");
    }

    return 0;
}

static int
fs_test_read_directory(char *root)
{
    struct fs_dir *dir;
    struct fs_dirent *dirent;
    char name[40];
    uint8_t out_len;
    int rc;

    printf("Opening (%s) directory... ", root);
    rc = fs_opendir(root, &dir);
    if (rc != 0) {
        printf("fail (%d)\n", rc);
        return -1;
    }
    printf("ok\n");

    do {
        printf("Reading directory entry... ");
        rc = fs_readdir(dir, &dirent);
        if (rc == FS_ENOENT) {
            printf("ok\n");
            rc = 0;
            break;
        } else if (rc != 0) {
            printf("fail (%d)\n", rc);
            rc = -1;
            goto out;
        }
        printf("ok\n");

        printf("Getting dirent information... ");
        rc = fs_dirent_name(dirent, 40, name, &out_len);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            rc = -1;
            goto out;
        }
        printf("ok\n");

        if (fs_dirent_is_dir(dirent)) {
            printf("Found directory (%s)\n", name);
        } else {
            printf("Found file (%s)\n", name);
        }
    } while (1);

out:
    fs_closedir(dir);

    return rc;
}

static int
fs_test_cleanup(char *root)
{
    int i;
    char name[30];
    int rc;

    memset(name, 0, sizeof(name));

    for (i = 0; i < MAX_TEST_FILES; i++) {
        sprintf(name, "%s/tested_%d", root, i);

        printf("Removing file (%s)... ", name);
        rc = fs_unlink(name);
        if (rc != 0) {
            printf("fail (%d)\n", rc);
            return -1;
        }
        printf("ok\n");
    }

    printf("Remove directory (%s)... ", root);
    rc = fs_unlink(root);
    if (rc < 0) {
        printf("fail (%d)\n", rc);
        return -1;
    }
    printf("ok\n");

    return 0;
}

extern int fs_lowlevel_init(void);

static void
fs_test_handler(void *arg)
{
    int rc;
    char root[30];

    g_blink_freq = BLINK_SLOW;

    printf("Will start test in %d secs...\n", STARTUP_DELAY);
    os_time_delay(STARTUP_DELAY * OS_TICKS_PER_SEC);

    rc = fs_lowlevel_init();
    if (rc == 0) {
        rc = fs_test_create_directory(root);
    }
    if (rc == 0) {
        rc = fs_test_write_files(root);
    }
    if (rc == 0) {
        rc = fs_test_read_files(root);
    }
    if (rc == 0) {
        rc = fs_test_rename_files(root);
    }
    if (rc == 0) {
        rc = fs_test_read_directory(root);
    }
    if (rc == 0) {
        rc = fs_test_cleanup(root);
    }

    if (rc) {
        printf("Filesystem testing has failed\n");
        g_blink_freq = BLINK_FAST;
    } else {
        printf("Filesystem testing was successful\n");
        g_blink_freq = BLINK_NORMAL;
    }

    while (1) {
        os_time_delay(1);
    }
}

static void
blinky_handler(void *arg)
{
    int led_pin;

    led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(led_pin, 1);

    while (1) {
        os_time_delay(g_blink_freq);
        hal_gpio_toggle(led_pin);
    }
}

static void
init_tasks(void)
{
    os_stack_t *pstack;

    pstack = malloc(sizeof(*pstack) * FS_TEST_STACK_SIZE);
    assert(pstack);

    os_task_init(&fs_test_task, "fs_test", fs_test_handler, NULL,
                 FS_TEST_PRIO, OS_WAIT_FOREVER, pstack, FS_TEST_STACK_SIZE);

    pstack = malloc(sizeof(*pstack) * BLINKY_STACK_SIZE);
    assert(pstack);

    os_task_init(&blinky_task, "blinky", blinky_handler, NULL,
                 BLINKY_PRIO, OS_WAIT_FOREVER, pstack, BLINKY_STACK_SIZE);
}

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    init_tasks();

    /*
     * As the last thing, process events from default event queue.
     */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
}
