/**
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

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "bootutil/image.h"
#include "imgmgr/imgmgr.h"
#include "crc32.h"

#if !defined(__BYTE_ORDER__) || (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#error "Machine must be little endian"
#endif

static void
print_usage(FILE *stream)
{
    fprintf(stream, "usage: bin2img <in-filename> <out-filename> <version>\n");
    fprintf(stream, "\n");
    fprintf(stream, "version numbers are of the form: XX.XX.XXXX.XXXXXXXX\n");
}

static int
is_image_file(const char *filename)
{
    FILE *fp;
    uint32_t magic;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        return 0;
    }

    magic = 0;
    fread(&magic, sizeof magic, 1, fp);

    fclose(fp);

    return magic == IMAGE_MAGIC;
}

int
main(int argc, char **argv)
{
    struct image_header hdr;
    struct stat st;
    uint8_t *buf;
    FILE *fpout;
    FILE *fpin;
    int crc_field_off;
    int crc_start;
    int crc_len;
    int rc;

    if (argc < 4) {
        print_usage(stderr);
        return 1;
    }

    memset(&hdr, 0, sizeof hdr);

    fpin = fopen(argv[1], "rb");
    if (fpin == NULL) {
        fprintf(stderr, "* error: could not open input file %s\n", argv[1]);
        print_usage(stderr);
        return 1;
    }

    if (is_image_file(argv[1])) {
        fprintf(stderr, "* error: source file is already an image (%s)\n",
                argv[1]);
        print_usage(stderr);
        return 1;
    }

    fpout = fopen(argv[2], "wb");
    if (fpout == NULL) {
        fprintf(stderr, "* error: could not open output file %s\n", argv[1]);
        print_usage(stderr);
        return 1;
    }

    rc = imgr_ver_parse(argv[3], &hdr.ih_ver);
    if (rc != 0) {
        print_usage(stderr);
        return 1;
    }

    rc = stat(argv[1], &st);
    if (rc != 0) {
        perror("stat");
        print_usage(stderr);
        return 1;
    }

    buf = malloc(sizeof hdr + st.st_size);
    assert(buf != NULL);

    rc = fread(buf + sizeof hdr, st.st_size, 1, fpin);
    if (rc != 1) {
        if (ferror(fpin)) {
            fprintf(stderr, "* error: file read error (%s) (file=%s)\n",
                    strerror(errno), argv[1]);
        } else {
            fprintf(stderr, "* error: file read error (inconsistent length) "
                            "(file=%s)\n", argv[1]);
        }
        print_usage(stderr);
        return 1;
    }

    hdr.ih_magic = IMAGE_MAGIC;
    hdr.ih_hdr_size = sizeof hdr;
    hdr.ih_img_size = st.st_size;
    memcpy(buf, &hdr, sizeof hdr);

    crc_field_off = offsetof(struct image_header, ih_crc32);
    crc_start = crc_field_off + sizeof hdr.ih_crc32;
    crc_len = sizeof hdr - crc_start + st.st_size;
    hdr.ih_crc32 = crc32(0, buf + crc_start, crc_len);
    memcpy(buf + crc_field_off, &hdr.ih_crc32, sizeof hdr.ih_crc32);

    rc = fwrite(buf, sizeof hdr + st.st_size, 1, fpout);
    if (rc != 1) {
        fprintf(stderr, "* error: file write error (file=%s)\n", argv[2]);
        print_usage(stderr);
        return 1;
    }

    return 0;
}

