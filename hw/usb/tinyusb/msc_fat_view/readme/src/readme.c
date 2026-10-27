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
#include <sysflash/sysflash.h>
#include <flash_map/flash_map.h>
#include <bootutil/image.h>
#include <stream/stream.h>
#include <msc_fat_view/msc_fat_view.h>
#include <msc_fat_view/readme.h>
#include <modlog/modlog.h>

#if MYNEWT_VAL(MSC_FAT_VIEW_DEFAULT_README_HEADER)
README_TEXT(00, "This device runs " MYNEWT_VAL(MSC_FAT_VIEW_DEFAULT_README_APP_NAME)
                " on " MYNEWT_VAL(BSP_NAME));
#endif

#if MYNEWT_VAL(MSC_FAT_VIEW_DEFAULT_README_VERSION)
static int
readme_app_version(struct out_stream *out, const void *arg)
{
    struct image_header image_header;
    const struct flash_area *fa;
    int rc = 0;

    if (flash_area_open(FLASH_AREA_IMAGE_0, &fa) == 0) {
        if (flash_area_read(fa, 0, &image_header, sizeof(image_header)) == 0 &&
            image_header.ih_magic == IMAGE_MAGIC) {
            rc = ostream_printf(out, "\nApp version: %u.%u.%u.%u\n",
                                image_header.ih_ver.iv_major,
                                image_header.ih_ver.iv_minor,
                                image_header.ih_ver.iv_revision,
                                (unsigned)image_header.ih_ver.iv_build_num);
        }
        flash_area_close(fa);
    }

    return rc;
}

README_ENTRY(10, readme_app_version, NULL);
#endif

#if MYNEWT_VAL(MSC_FAT_VIEW_DEFAULT_README_INCLUDE_HASHES)

#if defined MYNEWT_VAL_REPO_VERSION_APACHE_MYNEWT_CORE
#define REPO_VERSION_APACHE_MYNEWT_CORE     MYNEWT_VAL(REPO_VERSION_APACHE_MYNEWT_CORE)
#else
#define REPO_VERSION_APACHE_MYNEWT_CORE     "unknown"
#endif

#if defined MYNEWT_VAL_REPO_HASH_APACHE_MYNEWT_CORE
#define REPO_HASH_APACHE_MYNEWT_CORE        MYNEWT_VAL(REPO_HASH_APACHE_MYNEWT_CORE)
#else
#define REPO_HASH_APACHE_MYNEWT_CORE        ""
#endif

#if defined MYNEWT_VAL_REPO_HASH_APACHE_MYNEWT_NIMBLE
#define REPO_HASH_APACHE_MYNEWT_NIMBLE      MYNEWT_VAL(REPO_HASH_APACHE_MYNEWT_NIMBLE)
#else
#define REPO_HASH_APACHE_MYNEWT_NIMBLE      ""
#endif

#if defined MYNEWT_VAL_REPO_VERSION_TINYUSB
#define REPO_VERSION_TINYUSB                MYNEWT_VAL(REPO_VERSION_TINYUSB)
#else
#define REPO_VERSION_TINYUSB                "unknown"
#endif

#if defined MYNEWT_VAL_REPO_HASH_TINYUSB
#define REPO_HASH_TINYUSB                   MYNEWT_VAL(REPO_HASH_TINYUSB)
#else
#define REPO_HASH_TINYUSB                   ""
#endif

static const char readme_hashes[] =
    "\nmynewt: " REPO_VERSION_APACHE_MYNEWT_CORE " " REPO_HASH_APACHE_MYNEWT_CORE "\n"
#if defined MYNEWT_VAL_REPO_VERSION_APACHE_MYNEWT_NIMBLE
    "nimble: " MYNEWT_VAL(REPO_VERSION_APACHE_MYNEWT_NIMBLE) " " REPO_HASH_APACHE_MYNEWT_NIMBLE "\n"
#endif
    "tinyusb: " REPO_VERSION_TINYUSB " " REPO_HASH_TINYUSB "\n";

static int
readme_repo_hashes(struct out_stream *out, const void *arg)
{
    return ostream_write(out, (const uint8_t *)readme_hashes,
                         sizeof(readme_hashes) - 1, false);
}

README_ENTRY(20, readme_repo_hashes, NULL);
#endif

int
msc_fat_view_readme_text(struct out_stream *out, const void *arg)
{
    ostream_write_str(out, arg);

    return ostream_write_str(out, "\n");
}

int
msc_fat_view_readme_syscfg_str(struct out_stream *out, const void *arg)
{
    const struct msc_fat_view_readme_syscfg_str *syscfg = arg;

    ostream_write_str(out, syscfg->name);
    ostream_write_str(out, ": ");
    ostream_write_str(out, syscfg->value);

    return ostream_write_str(out, "\n");
}

int
msc_fat_view_readme_syscfg_int(struct out_stream *out, const void *arg)
{
    const struct msc_fat_view_readme_syscfg_int *syscfg = arg;

    return ostream_printf(out, "%s: %ld\n", syscfg->name, syscfg->value);
}

int
msc_fat_view_readme_syscfg_hex(struct out_stream *out, const void *arg)
{
    const struct msc_fat_view_readme_syscfg_int *syscfg = arg;

    return ostream_printf(out, "%s: 0x%lx\n", syscfg->name,
                          (unsigned long)syscfg->value);
}

/* Variable value zero extended to unsigned long */
static unsigned long
readme_var_read(const struct msc_fat_view_readme_var *var)
{
    switch (var->size) {
    case 1:
        return *(const volatile uint8_t *)var->ptr;
    case 2:
        return *(const volatile uint16_t *)var->ptr;
    case 4:
        return *(const volatile uint32_t *)var->ptr;
    default:
        return *(const volatile unsigned long *)var->ptr;
    }
}

int
msc_fat_view_readme_var_int(struct out_stream *out, const void *arg)
{
    const struct msc_fat_view_readme_var *var = arg;
    unsigned long value = readme_var_read(var);
    unsigned int bits = var->size * 8;

    if (var->is_signed) {
        /* Sign extend values narrower than long */
        if (bits < sizeof(long) * 8 && (value >> (bits - 1)) & 1) {
            value |= ~0UL << bits;
        }
        return ostream_printf(out, "%s: %ld\n", var->name, (long)value);
    }

    return ostream_printf(out, "%s: %lu\n", var->name, value);
}

int
msc_fat_view_readme_var_hex(struct out_stream *out, const void *arg)
{
    const struct msc_fat_view_readme_var *var = arg;

    return ostream_printf(out, "%s: 0x%lx\n", var->name, readme_var_read(var));
}

static void
readme_render(struct out_stream *out)
{
    LINK_TABLE_FOREACH(entry, msc_fat_view_readme) {
        entry->fn(out, entry->arg);
    }
}

static uint32_t
readme_size(const file_entry_t *file_entry)
{
    struct mem_out_stream mem;

    msc_fat_view_sector_stream_init(&mem, NULL, 0);
    readme_render((struct out_stream *)&mem);

    return mem.write_ptr;
}

static void
readme_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    struct mem_out_stream mem;

    MSC_FAT_VIEW_LOG_DEBUG("Readme read %d\n", file_sector);
    memset(buffer, 0, 512);
    msc_fat_view_sector_stream_init(&mem, buffer, file_sector);
    readme_render((struct out_stream *)&mem);
}

ROOT_DIR_ENTRY(readme, "README.TXT", FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY,
               readme_size, readme_read, NULL, NULL, NULL);
