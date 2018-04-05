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

#include <stddef.h>

#include "os/mynewt.h"

#include <tinycbor/cbor_mbuf_writer.h>
#include <tinycbor/cbor_mbuf_reader.h>

#include "oic/port/mynewt/config.h"
#include "oic/oc_rep.h"
#include "oic/oc_log.h"
#include "oic/port/mynewt/config.h"
#include "port/oc_assert.h"
#include "api/oc_priv.h"

#ifdef OC_CLIENT
static struct os_mempool oc_rep_objects;
static uint8_t oc_rep_objects_area[OS_MEMPOOL_BYTES(EST_NUM_REP_OBJECTS,
      sizeof(oc_rep_t))];
#endif

static struct os_mbuf *g_outm;
CborEncoder g_encoder, root_map, links_array;
CborError g_err;
struct cbor_mbuf_writer g_buf_writer;

void
oc_rep_new(struct os_mbuf *m)
{
    g_err = CborNoError;
    g_outm = m;
    cbor_mbuf_writer_init(&g_buf_writer, m);
    cbor_encoder_init(&g_encoder, &g_buf_writer.enc, 0);
}

int
oc_rep_finalize(void)
{
    int size = OS_MBUF_PKTLEN(g_outm);
    oc_rep_reset();
    if (g_err != CborNoError) {
        return -1;
    }
    return size;
}

void
oc_rep_reset(void)
{
    memset(&g_encoder, 0, sizeof(g_encoder));
}

#ifdef OC_CLIENT
static oc_rep_t *
_alloc_rep(void)
{
    oc_rep_t *rep = os_memblock_get(&oc_rep_objects);

    memset(rep, 0, sizeof(*rep));
#ifdef DEBUG
    oc_assert(rep != NULL);
#endif
    return rep;
}

static void
_free_rep(oc_rep_t *rep_value)
{
    os_memblock_put(&oc_rep_objects, rep_value);
}

void
oc_free_rep(oc_rep_t *rep)
{
    if (rep == NULL) {
        return;
    }
    oc_free_rep(rep->next);
    switch (rep->type) {
    case BYTE_STRING_ARRAY:
    case STRING_ARRAY:
        oc_free_string_array(&rep->value_array);
        break;
    case BOOL_ARRAY:
        oc_free_bool_array(&rep->value_array);
        break;
    case DOUBLE_ARRAY:
        oc_free_double_array(&rep->value_array);
        break;
    case INT_ARRAY:
        oc_free_int_array(&rep->value_array);
        break;
    case BYTE_STRING:
    case STRING:
        oc_free_string(&rep->value_string);
        break;
    case OBJECT:
        oc_free_rep(rep->value_object);
        break;
    case OBJECT_ARRAY:
        oc_free_rep(rep->value_object_array);
        break;
    default:
        break;
    }
    oc_free_string(&rep->name);
    _free_rep(rep);
}

/*
  An Object is a collection of key-value pairs.
  A value_object value points to the first key-value pair,
  and subsequent items are accessed via the next pointer.

  An Object Array is a collection of objects, where each object
  is a collection of key-value pairs.
  A value_object_array value points to the first object in the
  array. This object is then traversed via its value_object pointer.
  Subsequent objects in the object array are then accessed through
  the next pointer of the first object.
*/

/* Parse single property */
static void
oc_parse_rep_value(CborValue *value, oc_rep_t **rep, CborError *err)
{
  size_t k, len;
  CborValue map, array;
  *rep = _alloc_rep();
  oc_rep_t *cur = *rep, **prev = 0;
  cur->next = 0;
  cur->value_object_array = 0;
  /* key */
  *err |= cbor_value_calculate_string_length(value, &len);
  len++;
  oc_alloc_string(&cur->name, len);
  *err |= cbor_value_copy_text_string(value, oc_string(cur->name), &len, NULL);
  *err |= cbor_value_advance(value);
  /* value */
  switch (value->type) {
  case CborIntegerType:
    *err |= cbor_value_get_int64(value, &cur->value_int);
    cur->type = INT;
    break;
  case CborBooleanType:
    *err |= cbor_value_get_boolean(value, &cur->value_boolean);
    cur->type = BOOL;
    break;
  case CborDoubleType:
    *err |= cbor_value_get_double(value, &cur->value_double);
    cur->type = DOUBLE;
    break;
  case CborByteStringType:
    *err |= cbor_value_calculate_string_length(value, &len);
    len++;
    oc_alloc_string(&cur->value_string, len);
    *err |= cbor_value_copy_byte_string(value,
                                        (uint8_t *)oc_string(cur->value_string),
                                        &len, NULL);
    cur->type = BYTE_STRING;
    break;
  case CborTextStringType:
    *err |= cbor_value_calculate_string_length(value, &len);
    len++;
    oc_alloc_string(&cur->value_string, len);
    *err |= cbor_value_copy_text_string(value, oc_string(cur->value_string),
                                        &len, NULL);
    cur->type = STRING;
    break;
  case CborMapType: /* when value is a map/object */ {
    oc_rep_t **obj = &cur->value_object; // object points to list of properties
    *err |= cbor_value_enter_container(value, &map);
    while (!cbor_value_at_end(&map)) {
      oc_parse_rep_value(&map, obj, err);
      (*obj)->next = 0;
      obj = &(*obj)->next;
      *err |= cbor_value_advance(&map);
    }
    cur->type = OBJECT;
  } break;
  case CborArrayType:
    *err |= cbor_value_enter_container(value, &array);
    len = 0;
    cbor_value_get_array_length(value, &len);
    if (len == 0) {
      CborValue t = array;
      while (!cbor_value_at_end(&t)) {
        len++;
        cbor_value_advance(&t);
      }
    }
    k = 0;
    while (!cbor_value_at_end(&array)) {
      switch (array.type) {
      case CborIntegerType:
        if (k == 0) {
          oc_new_int_array(&cur->value_array, len);
          cur->type = INT | ARRAY;
        }
        *err |=
          cbor_value_get_int64(&array, oc_int_array(cur->value_array) + k);
        break;
      case CborDoubleType:
        if (k == 0) {
          oc_new_double_array(&cur->value_array, len);
          cur->type = DOUBLE | ARRAY;
        }
        *err |=
          cbor_value_get_double(&array, oc_double_array(cur->value_array) + k);
        break;
      case CborBooleanType:
        if (k == 0) {
          oc_new_bool_array(&cur->value_array, len);
          cur->type = BOOL | ARRAY;
        }
        *err |=
          cbor_value_get_boolean(&array, oc_bool_array(cur->value_array) + k);
        break;
      case CborByteStringType:
        if (k == 0) {
          oc_new_string_array(&cur->value_array, len);
          cur->type = BYTE_STRING | ARRAY;
        }
        *err |= cbor_value_calculate_string_length(&array, &len);
        len++;
        *err |= cbor_value_copy_byte_string(
          &array, (uint8_t *)oc_string_array_get_item(cur->value_array, k),
          &len, NULL);
        break;
      case CborTextStringType:
        if (k == 0) {
          oc_new_string_array(&cur->value_array, len);
          cur->type = STRING | ARRAY;
        }
        *err |= cbor_value_calculate_string_length(&array, &len);
        len++;
        *err |= cbor_value_copy_text_string(
          &array, (char *)oc_string_array_get_item(cur->value_array, k), &len,
          NULL);
        break;
      case CborMapType:
        if (k == 0) {
          cur->type = OBJECT | ARRAY;
          cur->value_object_array = _alloc_rep();
          prev = &cur->value_object_array;
        } else {
          (*prev)->next = _alloc_rep();
          prev = &(*prev)->next;
        }
        (*prev)->type = OBJECT;
        (*prev)->next = 0;
        oc_rep_t **obj = &(*prev)->value_object;
        /* Process a series of properties that make up an object of the array */
        *err |= cbor_value_enter_container(&array, &map);
        while (!cbor_value_at_end(&map)) {
          oc_parse_rep_value(&map, obj, err);
          obj = &(*obj)->next;
          *err |= cbor_value_advance(&map);
        }
        break;
      default:
        break;
      }
      k++;
      *err |= cbor_value_advance(&array);
    }
    break;
  default:
    break;
  }
}

uint16_t
oc_parse_rep(struct os_mbuf *m, uint16_t payload_off,
             uint16_t payload_size, oc_rep_t **out_rep)
{
  CborParser parser;
  CborValue root_value, cur_value, map;
  CborError err = CborNoError;
  struct cbor_mbuf_reader br;

  cbor_mbuf_reader_init(&br, m, payload_off);
  err |= cbor_parser_init(&br.r, 0, &parser, &root_value);
  if (cbor_value_is_map(&root_value)) {
    err |= cbor_value_enter_container(&root_value, &cur_value);
    *out_rep = 0;
    oc_rep_t **cur = out_rep;
    while (cbor_value_is_valid(&cur_value)) {
      oc_parse_rep_value(&cur_value, cur, &err);
      err |= cbor_value_advance(&cur_value);
      cur = &(*cur)->next;
    }
  } else if (cbor_value_is_array(&root_value)) {
    err |= cbor_value_enter_container(&root_value, &map);
    err |= cbor_value_enter_container(&map, &cur_value);
    *out_rep = 0;
    oc_rep_t **cur = out_rep;
    while (cbor_value_is_valid(&cur_value)) {
      *cur = _alloc_rep();
      (*cur)->type = OBJECT;
      oc_parse_rep_value(&cur_value, &(*cur)->value_object, &err);
      err |= cbor_value_advance(&cur_value);
      (*cur)->next = 0;
      cur = &(*cur)->next;
    }
  }
  return (uint16_t)err;
}

void
oc_rep_init(void)
{
    os_mempool_init(&oc_rep_objects, EST_NUM_REP_OBJECTS,
                    sizeof(oc_rep_t), oc_rep_objects_area, "oc_rep_o");
}
#endif
