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

#include <cborattr/cborattr.h>
#include <tinycbor/cbor.h>

/* this maps a CborType to a matching CborAtter Type. The mapping is not
 * one-to-one because of signedness of integers
 * and therefore we need a function to do this trickery */
static int
valid_attr_type(CborType ct, CborAttrType at) {
    switch (at) {
        case CborAttrIntegerType:
        case CborAttrUnsignedIntegerType:
            if(ct == CborIntegerType) {
                return 1;
            }
            break;
        case CborAttrByteStringType:
            if (ct == CborByteStringType) {
                return 1;
            }
            break;
        case CborAttrTextStringType:
            if (ct == CborTextStringType) {
                return 1;
            }
            break;
        case CborAttrBooleanType:
            if (ct == CborBooleanType) {
                return 1;
            }
#if FLOAT_SUPPORT
        case CborAttrFloatType:
            if (ct == CborFloatType) {
                return 1;
            }
            break;
        case CborAttrDoubleType:
            if (ct == CborDoubleType) {
                return 1;
            }
            break;
#endif
        case CborAttrArrayType:
            if (ct == CborArrayType) {
                return 1;
            }
            break;
        case CborAttrNullType:
            if (ct == CborNullType) {
                return 1;
            }
            break;
        default:
            break;
    }
    return 0;
}

/* this function find the pointer to the memory location to
  * write or read and attribute from the cbor_attr_r structure */
static char *
cbor_target_address(const struct cbor_attr_t *cursor,
        const struct cbor_array_t *parent, int offset)
{
    char *targetaddr = NULL;
    if (parent == NULL || parent->element_type != CborAttrArrayType) {
        /* ordinary case - use the address in the cursor structure */
        switch (cursor->type) {
        case CborAttrNullType:
            targetaddr = NULL;
            break;
        case CborAttrIntegerType:
            targetaddr = (char *)&cursor->addr.integer[offset];
            break;
        case CborAttrUnsignedIntegerType:
            targetaddr = (char *)&cursor->addr.uinteger[offset];
            break;
#if FLOAT_SUPPORT
        case CborAttrFloatType:
            targetaddr = (char *)&cursor->addr.fval[offset];
            break;
        case CborAttrDoubleType:
            targetaddr = (char *)&cursor->addr.real[offset];
            break;
#endif
        case CborAttrByteStringType:
            targetaddr = (char *) cursor->addr.bytestring.data;
            break;
        case CborAttrTextStringType:
            targetaddr = cursor->addr.string;
            break;
        case CborAttrBooleanType:
            targetaddr = (char *)&cursor->addr.boolean[offset];
            break;
        default:
            targetaddr = NULL;
            break;
        }
    } else {
        /* tricky case - hacking a member in an array of structures */
        targetaddr =
            parent->arr.objects.base + (offset * parent->arr.objects.stride) +
            cursor->addr.offset;
    }
    return targetaddr;
}

static int
cbor_internal_read_object(CborValue *root_value,
                          const struct cbor_attr_t *attrs,
                          const struct cbor_array_t *parent,
                          int offset) {
    const struct cbor_attr_t *cursor;
    char attrbuf[CBOR_ATTR_MAX + 1];
    char *lptr;
    CborValue cur_value;
    CborError g_err = 0;
    size_t len;
    CborType type = CborInvalidType;

    /* stuff fields with defaults in case they're omitted in the JSON input */
    for (cursor = attrs; cursor->attribute != NULL; cursor++) {
        if (!cursor->nodefault) {
            lptr = cbor_target_address(cursor, parent, offset);
            if (lptr != NULL) {
                switch (cursor->type) {
                    case CborAttrIntegerType:
                        memcpy(lptr, &cursor->dflt.integer, sizeof(long long int));
                        break;
                    case CborAttrUnsignedIntegerType:
                        memcpy(lptr, &cursor->dflt.integer, sizeof(long long unsigned int));
                        break;
                    case CborAttrBooleanType:
                        memcpy(lptr, &cursor->dflt.boolean, sizeof(bool));
                        break;
#if FLOAT_SUPPORT
                    case CborAttrFloatType:
                        memcpy(lptr, &cursor->dflt.fval, sizeof(float));
                        break;
                    case CborAttrDoubleType:
                        memcpy(lptr, &cursor->dflt.real, sizeof(double));
                        break;
#endif
                    default:
                        break;
                }
            }
        }
    }

    if (cbor_value_is_map(root_value)) {
        g_err |= cbor_value_enter_container(root_value, &cur_value);
    } else {
        g_err |= CborErrorIllegalType;
        return g_err;
    }

    /* contains key value pairs */
    while (cbor_value_is_valid(&cur_value)) {
        /* get the attribute */
        if (cbor_value_is_text_string(&cur_value)) {
            if (cbor_value_calculate_string_length(&cur_value, &len) == 0) {
                if (len > CBOR_ATTR_MAX) {
                    g_err |= CborErrorDataTooLarge;
                    goto g_err_return;
                }
                g_err |= cbor_value_copy_text_string(&cur_value, attrbuf, &len, NULL);
            }
        } else {
            g_err |= CborErrorIllegalType;
            goto g_err_return;
        }

        /* at least get the type of the next value so we can match the
         * attribute name and type for a perfect match */
        g_err |= cbor_value_advance(&cur_value);
        if(cbor_value_is_valid(&cur_value)) {
            type = cbor_value_get_type(&cur_value);
        } else {
            g_err |= CborErrorIllegalType;
            goto g_err_return;
        }

        /* find this attribute in our list */
        for (cursor = attrs; cursor->attribute != NULL; cursor++) {
            if ( valid_attr_type(type, cursor->type) &&
                (memcmp(cursor->attribute, attrbuf, len) == 0 )) {
                break;
            }
        }

        /* we found a match */
        if ( cursor->attribute != NULL ) {
           lptr = cbor_target_address(cursor, parent, offset);
            switch (cursor->type) {
                case CborAttrNullType:
                    /* nothing to do */
                    break;
                case CborAttrBooleanType:
                    g_err |= cbor_value_get_boolean(&cur_value, (bool *) lptr);
                    break;
                case CborAttrIntegerType:
                    g_err |= cbor_value_get_int64(&cur_value, (long long int *) lptr);
                    break;
                case CborAttrUnsignedIntegerType:
                    g_err |= cbor_value_get_uint64(&cur_value, (long long unsigned int *) lptr);
                    break;
#if FLOAT_SUPPORT
                case CborAttrFloatType:
                    g_err |= cbor_value_get_float(&cur_value, (float *) lptr);
                    break;
                case CborAttrDoubleType:
                    g_err |= cbor_value_get_double(&cur_value, (double *) lptr);
                    break;
#endif
                case CborAttrByteStringType:
                {
                    size_t len = cursor->len;
                    g_err |= cbor_value_copy_byte_string(&cur_value, (unsigned char *) lptr, &len, NULL);
                    *cursor->addr.bytestring.len = len;
                    break;
                }
                case CborAttrTextStringType:
                {
                    size_t len = cursor->len;
                    g_err |= cbor_value_copy_text_string(&cur_value, (char *) lptr, &len, NULL);
                    break;
                }
                /* TODO array */
                default:
                    g_err |= CborErrorIllegalType;
            }
        }
        cbor_value_advance(&cur_value);
    }
g_err_return:
    /* that should be it for this container */
    g_err |= cbor_value_leave_container(root_value, &cur_value);
    return g_err;
}

int
cbor_read_object(struct CborValue *value, const struct cbor_attr_t *attrs)
{
    int st;

    st = cbor_internal_read_object(value, attrs, NULL, 0);
    return st;
}
