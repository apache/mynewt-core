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

#include <stdlib.h>
#include <string.h>
#include "os/mynewt.h"
#include <disk/disk.h>

struct disk_info {
    const char *disk_name;
    const char *fs_name;
    struct disk_ops *dops;

    SLIST_ENTRY(disk_info) sc_next;
};

static SLIST_HEAD(, disk_info) disks = SLIST_HEAD_INITIALIZER();

/**
 *
 */
int disk_register(const char *disk_name, const char *fs_name, struct disk_ops *dops)
{
    struct disk_info *info = NULL;
    struct disk_info *sc;

    SLIST_FOREACH(sc, &disks, sc_next) {
        if (strcmp(sc->disk_name, disk_name) == 0) {
            return DISK_ENOENT;
        }
    }

    info = malloc(sizeof(struct disk_info));
    if (!info) {
        return DISK_ENOMEM;
    }

    info->disk_name = disk_name;
    info->fs_name = fs_name;
    info->dops = dops;

    SLIST_INSERT_HEAD(&disks, info, sc_next);

    return 0;
}

struct disk_ops *
disk_ops_for(const char *disk_name)
{
    struct disk_info *sc;

    if (disk_name) {
        SLIST_FOREACH(sc, &disks, sc_next) {
            if (strcmp(sc->disk_name, disk_name) == 0) {
                return sc->dops;
            }
        }
    }

    return NULL;
}

char *
disk_fs_for(const char *disk_name)
{
    struct disk_info *sc;

    if (disk_name) {
        SLIST_FOREACH(sc, &disks, sc_next) {
            if (strcmp(sc->disk_name, disk_name) == 0) {
                return ((char *) sc->fs_name);
            }
        }
    }

    return NULL;
}

char *
disk_name_from_path(const char *path)
{
    char *colon;
    uint8_t len;
    char *disk;

    colon = (char *) path;
    while (*colon && *colon != ':') {
        colon++;
    }

    if (*colon != ':') {
        return NULL;
    }

    len = colon - path;
    disk = malloc(len + 1);
    if (!disk) {
        return NULL;
    }
    memcpy(disk, path, len);
    disk[len] = '\0';

    return disk;
}

/**
 * @brief Returns the path with the disk prefix removed (if found)
 *
 * Paths should be given in the form disk<number>:/path. This routine
 * will parse and return only the path, removing the disk information.
 */
char *
disk_filepath_from_path(const char *path)
{
    char *colon;
    char *filepath;
    size_t len;

    colon = (char *) path;
    while (*colon && *colon != ':') {
        colon++;
    }

    if (*colon != ':') {
        filepath = strdup(path);
    } else {
        colon++;
        len = strlen(colon);
        filepath = malloc(len + 1);
        memcpy(filepath, colon, len);
        filepath[len] = '\0';
    }

    return filepath;
}
