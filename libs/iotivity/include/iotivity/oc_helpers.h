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

#include "util/oc_list.h"
#include "util/oc_mmem.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct oc_mmem oc_handle_t, oc_string_t, oc_array_t, oc_string_array_t;

#define oc_cast(block, type) ((type *)(OC_MMEM_PTR(&(block))))
#define oc_string(ocstring) (oc_cast(ocstring, char))

void oc_new_string(oc_string_t *ocstring, const char str[]);
void oc_alloc_string(oc_string_t *ocstring, int size);
void oc_free_string(oc_string_t *ocstring);
void oc_concat_strings(oc_string_t *concat, const char *str1, const char *str2);
#define oc_string_len(ocstring) ((ocstring).size ? (ocstring).size - 1 : 0)

void _oc_new_array(oc_array_t *ocarray, uint8_t size, pool type);
void _oc_free_array(oc_array_t *ocarray, pool type);
#define oc_new_int_array(ocarray, size) (_oc_new_array(ocarray, size, INT_POOL))
#define oc_new_bool_array(ocarray, size)                                       \
  (_oc_new_array(ocarray, size, BYTE_POOL))
#define oc_new_double_array(ocarray, size)                                     \
  (_oc_new_array(ocarray, size, DOUBLE_POOL))
#define oc_free_int_array(ocarray) (_oc_free_array(ocarray, INT_POOL))
#define oc_free_bool_array(ocarray) (_oc_free_array(ocarray, BYTE_POOL))
#define oc_free_double_array(ocarray) (_oc_free_array(ocarray, DOUBLE_POOL))
#define oc_int_array_size(ocintarray) ((ocintarray).size / sizeof(int64_t))
#define oc_bool_array_size(ocboolarray) ((ocboolarray).size / sizeof(bool))
#define oc_double_array_size(ocdoublearray)                                    \
  ((ocdoublearray).size / sizeof(double))
#define oc_int_array(ocintarray) (oc_cast(ocintarray, int64_t))
#define oc_bool_array(ocboolarray) (oc_cast(ocboolarray, bool))
#define oc_double_array(ocdoublearray) (oc_cast(ocdoublearray, double))

#define STRING_ARRAY_ITEM_MAX_LEN 24
void _oc_alloc_string_array(oc_string_array_t *ocstringarray, uint8_t size);
bool _oc_copy_string_to_string_array(oc_string_array_t *ocstringarray,
                                     const char str[], uint8_t index);
bool _oc_string_array_add_item(oc_string_array_t *ocstringarray,
                               const char str[]);
void oc_join_string_array(oc_string_array_t *ocstringarray,
                          oc_string_t *ocstring);
#define oc_new_string_array(ocstringarray, size)                               \
  (_oc_alloc_string_array(ocstringarray, size))
#define oc_free_string_array(ocstringarray) (oc_free_string(ocstringarray))
#define oc_string_array_add_item(ocstringarray, str)                           \
  (_oc_string_array_add_item(&(ocstringarray), str))
#define oc_string_array_get_item(ocstringarray, index)                         \
  (oc_string(ocstringarray) + index * STRING_ARRAY_ITEM_MAX_LEN)
#define oc_string_array_set_item(ocstringarray, str, index)                    \
  (_oc_copy_string_to_string_array(&(ocstringarray), str, index))
#define oc_string_array_get_item_size(ocstringarray, index)                    \
  (strlen((const char *)oc_string_array_get_item(ocstringarray, index)))
#define oc_string_array_get_allocated_size(ocstringarray)                      \
  ((ocstringarray).size / STRING_ARRAY_ITEM_MAX_LEN)

#endif /* OC_HELPERS_H */
