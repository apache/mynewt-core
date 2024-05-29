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
#include <stdbool.h>

#define SECTOR_SIZE                 512

#define FAT_FILE_ENTRY_ATTRIBUTE_FILE       0x00
#define FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY  0x01
#define FAT_FILE_ENTRY_ATTRIBUTE_HIDDEN     0x02
#define FAT_FILE_ENTRY_ATTRIBUTE_SYSTEM     0x04
#define FAT_FILE_ENTRY_ATTRIBUTE_LABEL      0x08
#define FAT_FILE_ENTRY_ATTRIBUTE_DIRECTORY  0x10
#define FAT_FILE_ENTRY_ATTRIBUTE_ARCHIVE    0x20

#define MSC_FAT_VIEW_FILE_ENTRY_VALID       0
#define MSC_FAT_VIEW_FILE_ENTRY_NOT_VALID   1

typedef struct file_entry {
    /** File name */
    const char *name;
    /** File attribute mask */
    uint8_t attributes;
    /** Function returning file size */
    uint32_t (*size)(const struct file_entry *entry);
    /** Function called when host tries to read file sector */
    void (*read_sector)(const struct file_entry *entry,
                        uint32_t file_sector, uint8_t buffer[512]);
    /** Function called when host tries to write file sector */
    void (*write_sector)(const struct file_entry *entry,
                         uint32_t file_sector, uint8_t buffer[512]);
    /** Function called when host deletes file */
    void (*delete_entry)(const struct file_entry *entry);
    /** Function called before entry is added to root folder,
     * allows to have statically created entry to be removed */
    int (*valid)(const struct file_entry *entry);
} file_entry_t;

typedef struct msc_fat_view_write_handler {
    int (*write_sector)(struct msc_fat_view_write_handler *handler,
                        uint32_t sector, uint8_t buffer[512]);
    int (*file_written)(struct msc_fat_view_write_handler *handler,
                        uint32_t size, uint32_t sector, bool first_sector);
} msc_fat_view_write_handler_t;

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

/* Section name for root entries */
#define ROOT_DIR_SECTION __attribute__((section(".msc_fat_view_root_entry"), used))

/* Section name for write handlers */
#define WRITE_HANDLER_SECTION __attribute__((section(".msc_fat_view_write_handlers"), used))

/**
 * Macro to add static root entries
 */
#define ROOT_DIR_ENTRY(entry, file_name, attr, size_fun, read_fun, write_fun, delete_fun, valid_fun) \
    file_entry_t entry = {                         \
        .name = file_name,                         \
        .attributes = attr,                        \
        .size = size_fun,                          \
        .read_sector = read_fun,                   \
        .write_sector = write_fun,                 \
        .delete_entry = delete_fun,                \
        .valid = valid_fun,                        \
    };                                             \
    const file_entry_t *entry ## _ptr ROOT_DIR_SECTION = &entry;

/**
 * Macro to add static write handlers
 */
#define MSC_FAT_VIEW_WRITE_HANDLER(entry, _write_sector, _file_written) \
    msc_fat_view_write_handler_t entry = {                      \
        .write_sector = _write_sector,                          \
        .file_written = _file_written,                          \
    };                                                          \
    const msc_fat_view_write_handler_t *entry ## _ptr WRITE_HANDLER_SECTION = &entry;

#define TABLE_START(table) __ ## table ## _start__
#define TABLE_END(table) __ ## table ## _end__

/**
 * Simple inline loop for link table tables
 *
 * @param table - name of table as defined in pkg.link_tables: section
 * @param type - type of element in table
 * @param fun - function to call for each element of the table.
 *              Function should have prototype like this: void fun(type *element)
 */
#define FOR_EACH_ENTRY(type, table, fun)           \
    {                                              \
        extern type const TABLE_START(table)[];    \
        extern type const TABLE_END(table)[];      \
        type *entry;                               \
        for (entry = (type *)TABLE_START(table);   \
             entry != TABLE_END(table); ++entry) { \
            fun(entry);                            \
        }                                          \
    }

#define FOR_TABLE(type, e, table)                  \
    extern type const TABLE_START(table)[];        \
    extern type const TABLE_END(table)[];          \
    for (type *e = (type *)TABLE_START(table); e != TABLE_END(table); ++e)

#endif /* __MSC_FAT_VIEW_H__ */
