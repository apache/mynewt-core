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

#ifndef _JSON_H_
#define _JSON_H_

#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif

#define JSON_VALUE_TYPE_BOOL   (0)
#define JSON_VALUE_TYPE_UINT64 (1)
#define JSON_VALUE_TYPE_INT64  (2)
#define JSON_VALUE_TYPE_STRING (3)
#define JSON_VALUE_TYPE_ARRAY  (4)
#define JSON_VALUE_TYPE_OBJECT (5)

struct json_value {
    uint8_t jv_pad1;
    uint8_t jv_type;
    uint16_t jv_len;

    union {
        uint64_t u;
        float fl;
        char *str;
        struct {
            char **keys;
            struct json_value **values;
        } composite;
    } jv_val;
};

#define JSON_VALUE_STRING(__jv, __str)        \
    (__jv)->jv_type = JSON_VALUE_TYPE_STRING; \
    (__jv)->jv_len = strlen(__str);           \
    (__jv)->jv_val.str = (__str);

#define JSON_VALUE_STRINGN(__jv, __str, __len) \
    (__jv)->jv_type = JSON_VALUE_TYPE_STRING;  \
    (__jv)->jv_len = (uint16_t) (__len);                    \
    (__jv)->jv_val.str = (__str);

#define JSON_VALUE_BOOL(__jv, __v)            \
    (__jv)->jv_type = JSON_VALUE_TYPE_BOOL;   \
    (__jv)->jv_val.u = (__v);

#define JSON_VALUE_INT(__jv, __v)             \
    (__jv)->jv_type = JSON_VALUE_TYPE_INT64;  \
    (__jv)->jv_val.u = (uint64_t) __v;

#define JSON_VALUE_UINT(__jv, __v)            \
    (__jv)->jv_type = JSON_VALUE_TYPE_UINT64; \
    (__jv)->jv_val.u = (uint64_t) __v;

/* Encoding functions */
typedef int (*json_write_func_t)(void *buf, char *data,
        int len);

struct json_encoder {
    json_write_func_t je_write;
    void *je_arg;
    int je_wr_commas:1;
    char je_encode_buf[64];
};


#define JSON_NITEMS(x) (int)(sizeof(x)/sizeof(x[0]))

int json_encode_object_start(struct json_encoder *);
int json_encode_object_key(struct json_encoder *encoder, char *key);
int json_encode_object_entry(struct json_encoder *, char *,
        struct json_value *);
int json_encode_object_finish(struct json_encoder *);

int json_encode_array_name(struct json_encoder *encoder, char *name);
int json_encode_array_start(struct json_encoder *encoder);
int json_encode_array_value(struct json_encoder *encoder, struct json_value *val);
int json_encode_array_finish(struct json_encoder *encoder);

/* Json parser definitions */
typedef enum {
    t_integer,
    t_uinteger,
    t_real,
    t_string,
    t_boolean,
    t_character,
    t_object,
    t_structobject,
    t_array,
    t_check,
    t_ignore
} json_type;

struct json_enum_t {
    char *name;
    long long int value;
};

struct json_array_t {
    json_type element_type;
    union {
        struct {
            const struct json_attr_t *subtype;
            char *base;
            size_t stride;
        } objects;
        struct {
            char **ptrs;
            char *store;
            int storelen;
        } strings;
        struct {
            long long int *store;
        } integers;
        struct {
            long long unsigned int *store;
        } uintegers;
        struct {
            double *store;
        } reals;
        struct {
            bool *store;
        } booleans;
    } arr;
    int *count;
    int maxlen;
};

struct json_attr_t {
    char *attribute;
    json_type type;
    union {
        long long int *integer;
        long long unsigned int *uinteger;
        double *real;
        char *string;
        bool *boolean;
        char *character;
        struct json_array_t array;
        size_t offset;
    } addr;
    union {
        long long int integer;
        long long unsigned int uinteger;
        double real;
        bool boolean;
        char character;
        char *check;
    } dflt;
    size_t len;
    const struct json_enum_t *map;
    bool nodefault;
};

struct json_buffer;

/* when you implement a json buffer, you must implement these functions */

/* returns the next character in the buffer or '\0'*/
typedef char (*json_buffer_read_next_byte_t)(struct json_buffer *);
/* returns the previous character in the buffer or '\0' */
typedef char (*json_buffer_read_prev_byte_t)(struct json_buffer *);
/* returns the number of characters read or zero */
typedef int (*json_buffer_readn_t)(struct json_buffer *, char *buf, int n);

struct json_buffer {
    json_buffer_readn_t jb_readn;
    json_buffer_read_next_byte_t jb_read_next;
    json_buffer_read_prev_byte_t jb_read_prev;
};

#define JSON_ATTR_MAX        31        /* max chars in JSON attribute name */
#define JSON_VAL_MAX        512        /* max chars in JSON value part */

int json_read_object(struct json_buffer *, const struct json_attr_t *);
int json_read_array(struct json_buffer *, const struct json_array_t *);

#define JSON_ERR_OBSTART     1   /* non-WS when expecting object start */
#define JSON_ERR_ATTRSTART   2   /* non-WS when expecting attrib start */
#define JSON_ERR_BADATTR     3   /* unknown attribute name */
#define JSON_ERR_ATTRLEN     4   /* attribute name too long */
#define JSON_ERR_NOARRAY     5   /* saw [ when not expecting array */
#define JSON_ERR_NOBRAK      6   /* array element specified, but no [ */
#define JSON_ERR_STRLONG     7   /* string value too long */
#define JSON_ERR_TOKLONG     8   /* token value too long */
#define JSON_ERR_BADTRAIL    9   /* garbage while expecting comma or } or ] */
#define JSON_ERR_ARRAYSTART  10  /* didn't find expected array start */
#define JSON_ERR_OBJARR      11  /* error while parsing object array */
#define JSON_ERR_SUBTOOLONG  12  /* too many array elements */
#define JSON_ERR_BADSUBTRAIL 13  /* garbage while expecting array comma */
#define JSON_ERR_SUBTYPE     14  /* unsupported array element type */
#define JSON_ERR_BADSTRING   15  /* error while string parsing */
#define JSON_ERR_CHECKFAIL   16  /* check attribute not matched */
#define JSON_ERR_NOPARSTR    17  /* can't support strings in parallel arrays */
#define JSON_ERR_BADENUM     18  /* invalid enumerated value */
#define JSON_ERR_QNONSTRING  19  /* saw quoted value when expecting nonstring */
#define JSON_ERR_NONQSTRING  19  /* didn't see quoted value when expecting string */
#define JSON_ERR_MISC        20  /* other data conversion error */
#define JSON_ERR_BADNUM      21  /* error while parsing a numerical argument */
#define JSON_ERR_NULLPTR     22  /* unexpected null value or attribute pointer */

/*
 * Use the following macros to declare template initializers for structobject
 * arrays.  Writing the equivalents out by hand is error-prone.
 *
 * JSON_STRUCT_OBJECT takes a structure name s, and a fieldname f in s.
 *
 * JSON_STRUCT_ARRAY takes the name of a structure array, a pointer to a an
 * initializer defining the subobject type, and the address of an integer to
 * store the length in.
 */
#define JSON_STRUCT_OBJECT(s, f)        .addr.offset = offsetof(s, f)
#define JSON_STRUCT_ARRAY(a, e, n) \
        .addr.array.element_type = t_structobject, \
        .addr.array.arr.objects.subtype = e, \
        .addr.array.arr.objects.base = (char*)a, \
        .addr.array.arr.objects.stride = sizeof(a[0]), \
        .addr.array.count = n, \
        .addr.array.maxlen = (int)(sizeof(a)/sizeof(a[0]))

#ifdef __cplusplus
}
#endif

#endif /* _JSON_H_ */

/**
 *   @} OSEncoding
 * @} OSJSON
 */

