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

#ifndef OC_HELPERS_H
#define OC_HELPERS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STRING_ARRAY_ITEM_MAX_LEN 24

typedef struct oc_string {
    uint16_t os_sz;
    uint8_t *os_str;
} oc_string_t;

typedef struct oc_array {
    uint16_t oa_sz;
    union {
        bool *b;
        int64_t *i;
        double *d;
        char *s;
    } oa_arr;
} oc_array_t;

typedef struct oc_array oc_string_array_t;

#define oc_string(ocstring) ((char *)(ocstring).os_str)

void oc_new_string(oc_string_t *ocstring, const char str[]);
void oc_alloc_string(oc_string_t *ocstring, int size);
void oc_free_string(oc_string_t *ocstring);
void oc_concat_strings(oc_string_t *concat, const char *, const char *);
#define oc_string_len(ocstring) ((ocstring).os_sz ? (ocstring).os_sz - 1 : 0)

void _oc_new_array(oc_array_t *ocarray, uint8_t size, uint8_t elem_sz);
void _oc_free_array(oc_array_t *ocarray);

#define oc_new_int_array(ocarray, size)                                 \
    _oc_new_array(ocarray, size, sizeof(uint64_t))
#define oc_new_bool_array(ocarray, size)                                \
    _oc_new_array(ocarray, size, sizeof(bool))
#define oc_new_double_array(ocarray, size)                              \
    _oc_new_array(ocarray, size, sizeof(double))
#define oc_free_int_array(ocarray) _oc_free_array(ocarray)
#define oc_free_bool_array(ocarray) _oc_free_array(ocarray)
#define oc_free_double_array(ocarray) _oc_free_array(ocarray)
#define oc_int_array_size(ocintarray) ((ocintarray).oa_sz / sizeof(int64_t))
#define oc_bool_array_size(ocboolarray) ((ocboolarray).oa_sz / sizeof(bool))
#define oc_double_array_size(ocdoublearray)                                    \
  ((ocdoublearray).oa_sz / sizeof(double))
#define oc_int_array(ocintarray) (ocintarray.oa_arr.i)
#define oc_bool_array(ocboolarray) (ocboolarray.oa_arr.b)
#define oc_double_array(ocdoublearray) (ocdoublearray.oa_arr.d)

void _oc_alloc_string_array(oc_string_array_t *ocstringarray, uint8_t size);
bool _oc_copy_string_to_string_array(oc_string_array_t *ocstringarray,
                                     const char str[], uint8_t index);
bool _oc_string_array_add_item(oc_string_array_t *ocstringarray,
                               const char str[]);
#define oc_new_string_array(ocstringarray, size)                        \
    (_oc_alloc_string_array(ocstringarray, size))
#define oc_free_string_array(ocs) (_oc_free_array(ocs))
#define oc_string_array_add_item(ocstringarray, str)                    \
    (_oc_string_array_add_item(&(ocstringarray), str))
#define oc_string_array_get_item(ocstringarray, index)                  \
    (&(ocstringarray.oa_arr.s[index * STRING_ARRAY_ITEM_MAX_LEN]))
#define oc_string_array_set_item(ocstringarray, str, index)             \
    (_oc_copy_string_to_string_array(&(ocstringarray), str, index))
#define oc_string_array_get_item_size(ocstringarray, index)             \
    (strlen(oc_string_array_get_item(ocstringarray, index)))
#define oc_string_array_get_allocated_size(ocstringarray)               \
    (ocstringarray.oa_sz / STRING_ARRAY_ITEM_MAX_LEN)

#ifdef __cplusplus
}
#endif

#endif /* OC_HELPERS_H */
