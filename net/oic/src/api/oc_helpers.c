/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <stdbool.h>
#include <stdlib.h>

#include "os/mynewt.h"

#include "oic/port/mynewt/config.h"
#include "oic/oc_helpers.h"

void
oc_new_string(oc_string_t *os, const char str[])
{
    int len = strlen(str);

    os->os_str = malloc(len + 1);
    if (os->os_str) {
        os->os_sz = len + 1;
        memcpy(os->os_str, str, len);
        os->os_str[len] = '\0';
    }
}

void
oc_alloc_string(oc_string_t *os, int size)
{
    os->os_str = malloc(size);
    if (os->os_str) {
        os->os_sz = size;
    }
}

void
oc_free_string(oc_string_t *os)
{
    free(os->os_str);
    os->os_sz = 0;
}

void
oc_concat_strings(oc_string_t *concat, const char *str1, const char *str2)
{
    size_t len1 = strlen(str1), len2 = strlen(str2);

    oc_alloc_string(concat, len1 + len2 + 1);
    memcpy(concat->os_str, str1, len1);
    memcpy(&concat->os_str[len1], str2, len2);
    concat->os_str[len1 + len2] = '\0';
}

void
_oc_new_array(oc_array_t *oa, uint8_t size, uint8_t elem_size)
{
    oa->oa_arr.b = malloc(size * elem_size);
    if (oa->oa_arr.b) {
        oa->oa_sz = size * elem_size;
    }
}

void _oc_free_array(oc_array_t *oa)
{
    free(oa->oa_arr.b);
    oa->oa_sz = 0;
}

void
_oc_alloc_string_array(oc_string_array_t *osa, uint8_t size)
{
    int i;
    int pos;

    _oc_new_array(osa, size, STRING_ARRAY_ITEM_MAX_LEN);
    if (osa->oa_arr.s) {
        for (i = 0; i < size; i++) {
            pos = i * STRING_ARRAY_ITEM_MAX_LEN;
            osa->oa_arr.s[pos] = '\0';
        }
    }
}

bool
_oc_copy_string_to_string_array(oc_string_array_t *osa,
                                const char str[], uint8_t idx)
{
    int len;
    int pos;

    len = strlen(str);
    pos = idx * STRING_ARRAY_ITEM_MAX_LEN;

    if (len >= STRING_ARRAY_ITEM_MAX_LEN ||
        pos + STRING_ARRAY_ITEM_MAX_LEN > osa->oa_sz) {
        return false;
    }
    memcpy(&osa->oa_arr.s[pos], str, len);
    osa->oa_arr.s[pos + len] = '\0';
    return true;
}

bool
_oc_string_array_add_item(oc_string_array_t *osa, const char str[])
{
    int i;
    int sz;
    int pos;

    sz = osa->oa_sz / STRING_ARRAY_ITEM_MAX_LEN;
    for (i = 0; i < sz; i++) {
        pos = i * STRING_ARRAY_ITEM_MAX_LEN;
        if (osa->oa_arr.s[pos] == '\0') {
            _oc_copy_string_to_string_array(osa, str, i);
            return true;
        }
    }
    return false;
}
