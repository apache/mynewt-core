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

#include <string.h>
#include <msc_fat_view/msc_fat_view.h>

static const char mynewt_htm_text[] =
    "<!-- mynewt Website and Authentication Shortcut -->\n"
    "<html>\n"
    "<head>\n"
    "<meta http-equiv=\"refresh\" content=\"0; url=https://mynewt.apache.org/\"/>\n"
    "<title>mynewt Website Shortcut</title>\n"
    "</head>\n"
    "<body></body>\n"
    "</html>";

static uint32_t
mynewt_htm_size(const file_entry_t *file)
{
    (void)file;

    return strlen(mynewt_htm_text);
}

static void
mynewt_htm_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    (void)entry;

    if (file_sector == 0) {
        strcpy((char *)buffer, mynewt_htm_text);
        memset(buffer + 512 - sizeof(mynewt_htm_text), 0, sizeof(mynewt_htm_text));
    } else {
        memset(buffer, 0, 512);
    }
}

ROOT_DIR_ENTRY(mynew_htm, "MYNEWT.HTM", FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY, mynewt_htm_size, mynewt_htm_read, NULL, NULL);
