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

#include <stdio.h>
#include <sysflash/sysflash.h>
#include <msc_fat_view/msc_fat_view.h>
#include <bootutil/image.h>

#if MYNEWT_VAL(MSC_FAT_VIEW_HUGE_FILE_SIZE) > 0
#define HUGE_FILE_SIZE          MYNEWT_VAL(MSC_FAT_VIEW_HUGE_FILE_SIZE)
#if HUGE_FILE_SIZE > (MYNEWT_VAL(MSC_FAT_VIEW_DISK_SIZE) * 1024) + 2000000
#error HUGE_FILE_SIZE is to big for specified disk size
#endif
#elif (MYNEWT_VAL(MSC_FAT_VIEW_DISK_SIZE) * 1024) < 2000000
#error No space for huge file, increase MSC_FAT_VIEW_DISK_SIZE in syscfg
#else
#define HUGE_FILE_SIZE          ((MYNEWT_VAL(MSC_FAT_VIEW_DISK_SIZE) * 1024) - 2000000)
#endif

static uint32_t
huge_file_size(const file_entry_t *file)
{
    (void)file;

    return HUGE_FILE_SIZE;
}

static void
huge_file_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    (void)entry;

    memset(buffer, (uint8_t)file_sector, 512);
}

ROOT_DIR_ENTRY(huge_file, "Huge file", FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY, huge_file_size, huge_file_read, NULL, NULL, NULL);
