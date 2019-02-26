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

#include <ctype.h>
#include <string.h>

#include "config/config.h"
#include "config_priv.h"

int
conf_line_parse(char *buf, char **namep, char **valp)
{
    char *cp;
    enum {
        FIND_NAME,
        FIND_NAME_END,
        FIND_VAL,
        FIND_VAL_END
    } state = FIND_NAME;

    *valp = NULL;
    for (cp = buf; *cp != '\0'; cp++) {
        switch (state) {
        case FIND_NAME:
            if (!isspace((unsigned char)*cp)) {
                *namep = cp;
                state = FIND_NAME_END;
            }
            break;
        case FIND_NAME_END:
            if (*cp == '=') {
                *cp = '\0';
                state = FIND_VAL;
            } else if (isspace((unsigned char)*cp)) {
                *cp = '\0';
            }
            break;
        case FIND_VAL:
            if (!isspace((unsigned char)*cp)) {
                *valp = cp;
                state = FIND_VAL_END;
            }
            break;
        case FIND_VAL_END:
            if (!isprint((unsigned char)*cp)) {
                *cp = '\0';
            }
            break;
        }
    }
    if (state == FIND_VAL_END || state == FIND_VAL) {
        return 0;
    } else {
        return -1;
    }
}

int
conf_line_make(char *dst, int dlen, const char *name, const char *value)
{
    int nlen;
    int vlen;
    int off;

    nlen = strlen(name);
    if (value) {
        vlen = strlen(value);
    } else {
        vlen = 0;
    }
    if (nlen + vlen + 2 > dlen) {
        return -1;
    }
    memcpy(dst, name, nlen);
    off = nlen;
    dst[off++] = '=';

    memcpy(dst + off, value, vlen);
    off += vlen;
    dst[off] = '\0';

    return off;
}
