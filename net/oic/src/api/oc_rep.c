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
    cbor_writer_encoder_init(&g_buf_writer.enc, &g_encoder);
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

    if (rep) {
        memset(rep, 0, sizeof(*rep));
    }
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

/* Maximum nesting level of objects in parsed payload */
#define OC_REP_MAX_NESTING 8

static CborError oc_parse_rep_object(CborValue *value, oc_rep_t **rep, int depth);

/* Copies byte or text string value into newly allocated string */
static CborError
oc_parse_rep_string(CborValue *value, oc_string_t *str)
{
    size_t len;
    CborError err;

    err = cbor_mbuf_value_calculate_string_length(value, &len);
    if (err) {
        return err;
    }
    /* Reserve space for terminating NUL, size must fit in os_sz */
    if (len >= UINT16_MAX) {
        return CborErrorDataTooLarge;
    }
    len++;

    oc_alloc_string(str, len);
    if (!oc_string(*str)) {
        return CborErrorOutOfMemory;
    }

    if (cbor_value_is_text_string(value)) {
        err = cbor_mbuf_value_copy_text_string(value, oc_string(*str), &len, NULL);
    } else {
        err = cbor_mbuf_value_copy_byte_string(value, (uint8_t *)oc_string(*str),
                                               &len, NULL);
    }
    if (err) {
        return err;
    }

    return cbor_mbuf_value_advance(value);
}

static CborError
oc_parse_rep_string_array(CborValue *array, oc_rep_t *cur, size_t len,
                          oc_rep_value_type_t type)
{
    CborError err;
    size_t slen;
    size_t k;

    oc_new_string_array(&cur->value_array, len);
    if (!cur->value_array.oa_arr.s) {
        return CborErrorOutOfMemory;
    }
    cur->type = type;

    for (k = 0; k < len; k++) {
        err = cbor_mbuf_value_calculate_string_length(array, &slen);
        if (err) {
            return err;
        }
        /* Reserve space for terminating NUL */
        if (slen >= STRING_ARRAY_ITEM_MAX_LEN) {
            return CborErrorDataTooLarge;
        }
        slen = STRING_ARRAY_ITEM_MAX_LEN;

        if (type == STRING_ARRAY) {
            err = cbor_mbuf_value_copy_text_string(
                array, oc_string_array_get_item(cur->value_array, k), &slen, NULL);
        } else {
            err = cbor_mbuf_value_copy_byte_string(
                array, (uint8_t *)oc_string_array_get_item(cur->value_array, k),
                &slen, NULL);
        }
        if (err) {
            return err;
        }

        err = cbor_mbuf_value_advance(array);
        if (err) {
            return err;
        }
    }

    return CborNoError;
}

static CborError
oc_parse_rep_object_array(CborValue *array, oc_rep_t *cur, int depth)
{
    oc_rep_t **obj;
    CborError err;

    cur->type = OBJECT_ARRAY;
    obj = &cur->value_object_array;

    while (!cbor_value_at_end(array)) {
        *obj = _alloc_rep();
        if (!*obj) {
            return CborErrorOutOfMemory;
        }
        (*obj)->type = OBJECT;

        err = oc_parse_rep_object(array, &(*obj)->value_object, depth);
        if (err) {
            return err;
        }

        obj = &(*obj)->next;
    }

    return CborNoError;
}

static CborError
oc_parse_rep_array(CborValue *value, oc_rep_t *cur, int depth)
{
    CborValue array;
    CborValue t;
    CborType type;
    CborError err;
    size_t len;
    size_t k;

    err = cbor_value_enter_container(value, &array);
    if (err) {
        return err;
    }

    /* All elements must be of the same type */
    type = cbor_value_get_type(&array);
    len = 0;
    t = array;
    while (!cbor_value_at_end(&t)) {
        if (cbor_value_get_type(&t) != type) {
            return CborErrorIllegalType;
        }
        err = cbor_mbuf_value_advance(&t);
        if (err) {
            return err;
        }
        len++;
    }

    switch (type) {
    case CborIntegerType:
        oc_new_int_array(&cur->value_array, len);
        if (!cur->value_array.oa_arr.i) {
            return CborErrorOutOfMemory;
        }
        cur->type = INT_ARRAY;
        for (k = 0; k < len; k++) {
            err = cbor_value_get_int64(&array, &cur->value_array.oa_arr.i[k]);
            if (!err) {
                err = cbor_mbuf_value_advance(&array);
            }
            if (err) {
                return err;
            }
        }
        break;
    case CborDoubleType:
        oc_new_double_array(&cur->value_array, len);
        if (!cur->value_array.oa_arr.d) {
            return CborErrorOutOfMemory;
        }
        cur->type = DOUBLE_ARRAY;
        for (k = 0; k < len; k++) {
            err = cbor_value_get_double(&array, &cur->value_array.oa_arr.d[k]);
            if (!err) {
                err = cbor_mbuf_value_advance(&array);
            }
            if (err) {
                return err;
            }
        }
        break;
    case CborBooleanType:
        oc_new_bool_array(&cur->value_array, len);
        if (!cur->value_array.oa_arr.b) {
            return CborErrorOutOfMemory;
        }
        cur->type = BOOL_ARRAY;
        for (k = 0; k < len; k++) {
            err = cbor_value_get_boolean(&array, &cur->value_array.oa_arr.b[k]);
            if (!err) {
                err = cbor_mbuf_value_advance(&array);
            }
            if (err) {
                return err;
            }
        }
        break;
    case CborByteStringType:
        err = oc_parse_rep_string_array(&array, cur, len, BYTE_STRING_ARRAY);
        break;
    case CborTextStringType:
        err = oc_parse_rep_string_array(&array, cur, len, STRING_ARRAY);
        break;
    case CborMapType:
        err = oc_parse_rep_object_array(&array, cur, depth);
        break;
    default:
        /* Empty array or unsupported element type, skip it */
        return cbor_mbuf_value_advance(value);
    }
    if (err) {
        return err;
    }

    return cbor_value_leave_container(value, &array);
}

/* Parse single property */
static CborError
oc_parse_rep_value(CborValue *value, oc_rep_t **rep, int depth)
{
    oc_rep_t *cur;
    CborError err;

    cur = _alloc_rep();
    if (!cur) {
        return CborErrorOutOfMemory;
    }
    *rep = cur;

    /* key */
    if (!cbor_value_is_text_string(value)) {
        return CborErrorIllegalType;
    }
    err = oc_parse_rep_string(value, &cur->name);
    if (err) {
        return err;
    }

    /* value */
    switch (cbor_value_get_type(value)) {
    case CborIntegerType:
        err = cbor_value_get_int64(value, &cur->value_int);
        cur->type = INT;
        break;
    case CborBooleanType:
        err = cbor_value_get_boolean(value, &cur->value_boolean);
        cur->type = BOOL;
        break;
    case CborDoubleType:
        err = cbor_value_get_double(value, &cur->value_double);
        cur->type = DOUBLE;
        break;
    case CborByteStringType:
        cur->type = BYTE_STRING;
        return oc_parse_rep_string(value, &cur->value_string);
    case CborTextStringType:
        cur->type = STRING;
        return oc_parse_rep_string(value, &cur->value_string);
    case CborMapType:
        cur->type = OBJECT;
        return oc_parse_rep_object(value, &cur->value_object, depth);
    case CborArrayType:
        return oc_parse_rep_array(value, cur, depth);
    default:
        /* Unsupported value type, skip it */
        break;
    }
    if (err) {
        return err;
    }

    return cbor_mbuf_value_advance(value);
}

/* Parse map into list of properties */
static CborError
oc_parse_rep_object(CborValue *value, oc_rep_t **rep, int depth)
{
    CborValue map;
    CborError err;

    if (!cbor_value_is_map(value)) {
        return CborErrorIllegalType;
    }
    if (depth >= OC_REP_MAX_NESTING) {
        return CborErrorNestingTooDeep;
    }

    err = cbor_value_enter_container(value, &map);
    if (err) {
        return err;
    }

    while (!cbor_value_at_end(&map)) {
        err = oc_parse_rep_value(&map, rep, depth + 1);
        if (err) {
            return err;
        }
        rep = &(*rep)->next;
    }

    return cbor_value_leave_container(value, &map);
}

static CborError
oc_parse_rep_payload(struct os_mbuf *m, uint16_t payload_off, oc_rep_t **out_rep)
{
    CborParser parser;
    CborValue root_value;
    CborValue array;
    CborError err;
    struct cbor_mbuf_reader br;
    oc_rep_t **cur;

    *out_rep = NULL;

    cbor_mbuf_reader_init(&br, m, payload_off);
    err = cbor_reader_parser_init(&br.r, &parser, &root_value);
    if (err) {
        return err;
    }

    if (cbor_value_is_map(&root_value)) {
        return oc_parse_rep_object(&root_value, out_rep, 0);
    }

    if (!cbor_value_is_array(&root_value)) {
        return CborErrorIllegalType;
    }

    err = cbor_value_enter_container(&root_value, &array);
    if (err) {
        return err;
    }

    cur = out_rep;
    while (!cbor_value_at_end(&array)) {
        *cur = _alloc_rep();
        if (!*cur) {
            return CborErrorOutOfMemory;
        }
        (*cur)->type = OBJECT;

        err = oc_parse_rep_object(&array, &(*cur)->value_object, 1);
        if (err) {
            return err;
        }

        cur = &(*cur)->next;
    }

    return CborNoError;
}

/*
 * Parses payload into tree of oc_rep_t. Payload must be a map or an array of
 * maps. On error *out_rep may point to partially parsed payload, it must be
 * freed with oc_free_rep() regardless of returned value.
 *
 * Returns 0 on success, non-zero on failure.
 */
uint16_t
oc_parse_rep(struct os_mbuf *m, uint16_t payload_off, uint16_t payload_size,
             oc_rep_t **out_rep)
{
    /* CborError values do not fit in uint16_t */
    return oc_parse_rep_payload(m, payload_off, out_rep) != CborNoError;
}

void
oc_rep_init(void)
{
    os_mempool_init(&oc_rep_objects, EST_NUM_REP_OBJECTS,
                    sizeof(oc_rep_t), oc_rep_objects_area, "oc_rep_o");
}
#endif
