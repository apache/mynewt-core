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

#ifndef __MSC_FAT_VIEW_H__
#define __MSC_FAT_VIEW_H__

#include <inttypes.h>

#define FAT_FILE_ENTRY_ATTRIBUTE_FILE       0x00
#define FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY  0x01
#define FAT_FILE_ENTRY_ATTRIBUTE_HIDDEN     0x02
#define FAT_FILE_ENTRY_ATTRIBUTE_SYSTEM     0x04
#define FAT_FILE_ENTRY_ATTRIBUTE_LABEL      0x08
#define FAT_FILE_ENTRY_ATTRIBUTE_DIRECTORY  0x10
#define FAT_FILE_ENTRY_ATTRIBUTE_ARCHIVE    0x20

typedef struct file_entry {
    /** File name */
    const char *name;
    /** File attribute mask */
    uint8_t attributes;
    /** Function returning file size */
    uint32_t (*size)(const struct file_entry *entry);
    /** Function called when host tries to read file sector */
    void (*read_sector)(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512]);
    /** Function called when host tries to write file sector */
    void (*write_sector)(const struct file_entry *entry, uint32_t file_sector, const uint8_t buffer[512]);
    /** Function called when host deletes file */
    void (*delete_entry)(const struct file_entry *entry);
} file_entry_t;

/**
 * Add file handler to root folder.
 *
 * If medium was inserted this function will first eject media
 * to make system aware that disk content has changed and reload
 * is needed.
 *
 * @param file - File entry that will popup in root folder.
 */
void msc_fat_view_add_dir_entry(const file_entry_t *file);

/**
 * Eject media
 *
 * This function can be called before new root dir entries are added
 * in application.
 * Several entries can be added without generating too many disk ejected
 * notification in host system.
 */
void msc_fat_view_media_eject(void);

/**
 * Insert media after all root entries are added.
 */
void msc_fat_view_media_insert(void);

#endif /* __MSC_FAT_VIEW_H__ */
