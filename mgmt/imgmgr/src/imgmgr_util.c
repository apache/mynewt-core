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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bootutil/image.h>
#include <imgmgr/imgmgr.h>

int
imgr_ver_parse(char *src, struct image_version *ver)
{
    unsigned long ul;
    char *tok;
    char *nxt;
    char *ep;

    memset(ver, 0, sizeof(*ver));

    nxt = src;
    tok = strsep(&nxt, ".");
    ul = strtoul(tok, &ep, 10);
    if (tok[0] == '\0' || ep[0] != '\0' || ul > UINT8_MAX) {
        return -1;
    }
    ver->iv_major = ul;
    if (nxt == NULL) {
        return 0;
    }
    tok = strsep(&nxt, ".");
    ul = strtoul(tok, &ep, 10);
    if (tok[0] == '\0' || ep[0] != '\0' || ul > UINT8_MAX) {
        return -1;
    }
    ver->iv_minor = ul;
    if (nxt == NULL) {
        return 0;
    }

    tok = strsep(&nxt, ".");
    ul = strtoul(tok, &ep, 10);
    if (tok[0] == '\0' || ep[0] != '\0' || ul > UINT16_MAX) {
        return -1;
    }
    ver->iv_revision = ul;
    if (nxt == NULL) {
        return 0;
    }

    tok = nxt;
    ul = strtoul(tok, &ep, 10);
    if (tok[0] == '\0' || ep[0] != '\0' || ul > UINT32_MAX) {
        return -1;
    }
    ver->iv_build_num = ul;

    return 0;
}

int
imgr_ver_str(struct image_version *ver, char *dst)
{
    if (ver->iv_build_num) {
        return sprintf(dst, "%u.%u.%u.%lu",
          ver->iv_major, ver->iv_minor, ver->iv_revision,
          (unsigned long)ver->iv_build_num);
    } else {
        return sprintf(dst, "%u.%u.%u",
          ver->iv_major, ver->iv_minor, ver->iv_revision);
    }
}

