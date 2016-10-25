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

#include <stdio.h>
#include <string.h>

#include <json/json.h>

#define JSON_ENCODE_OBJECT_START(__e) \
    (__e)->je_write((__e)->je_arg, "{", sizeof("{")-1);

#define JSON_ENCODE_OBJECT_END(__e) \
    (__e)->je_write((__e)->je_arg, "}", sizeof("}")-1);

#define JSON_ENCODE_ARRAY_START(__e) \
    (__e)->je_write((__e)->je_arg, "[", sizeof("[")-1);

#define JSON_ENCODE_ARRAY_END(__e) \
    (__e)->je_write((__e)->je_arg, "]", sizeof("]")-1);


int
json_encode_object_start(struct json_encoder *encoder)
{
    if (encoder->je_wr_commas) {
        encoder->je_write(encoder->je_arg, ",", sizeof(",")-1);
        encoder->je_wr_commas = 0;
    }
    JSON_ENCODE_OBJECT_START(encoder);
    encoder->je_wr_commas = 0;

    return (0);
}

static int
json_encode_value(struct json_encoder *encoder, struct json_value *jv)
{
    int rc;
    int i;
    int len;

    switch (jv->jv_type) {
        case JSON_VALUE_TYPE_BOOL:
            len = sprintf(encoder->je_encode_buf, "%s",
                    jv->jv_val.u > 0 ? "true" : "false");
            encoder->je_write(encoder->je_arg, encoder->je_encode_buf, len);
            break;
        case JSON_VALUE_TYPE_UINT64:
            len = sprintf(encoder->je_encode_buf, "%llu",
                    jv->jv_val.u);
            encoder->je_write(encoder->je_arg, encoder->je_encode_buf, len);
            break;
        case JSON_VALUE_TYPE_INT64:
            len = sprintf(encoder->je_encode_buf, "%lld",
                    jv->jv_val.u);
            encoder->je_write(encoder->je_arg, encoder->je_encode_buf, len);
            break;
        case JSON_VALUE_TYPE_STRING:
            encoder->je_write(encoder->je_arg, "\"", sizeof("\"")-1);
            for (i = 0; i < jv->jv_len; i++) {
                switch (jv->jv_val.str[i]) {
                    case '"':
                    case '/':
                    case '\\':
                        encoder->je_write(encoder->je_arg, "\\",
                                sizeof("\\")-1);
                        encoder->je_write(encoder->je_arg,
                                (char *) &jv->jv_val.str[i], 1);

                        break;
                    case '\t':
                        encoder->je_write(encoder->je_arg, "\\t",
                                sizeof("\\t")-1);
                        break;
                    case '\r':
                        encoder->je_write(encoder->je_arg, "\\r",
                                sizeof("\\r")-1);
                        break;
                    case '\n':
                        encoder->je_write(encoder->je_arg, "\\n",
                                sizeof("\\n")-1);
                        break;
                    case '\f':
                        encoder->je_write(encoder->je_arg, "\\f",
                                sizeof("\\f")-1);
                        break;
                    case '\b':
                        encoder->je_write(encoder->je_arg, "\\b",
                                sizeof("\\b")-1);
                        break;
                   default:
                        encoder->je_write(encoder->je_arg,
                                (char *) &jv->jv_val.str[i], 1);
                        break;
                }

            }
            encoder->je_write(encoder->je_arg, "\"", sizeof("\"")-1);
            break;
        case JSON_VALUE_TYPE_ARRAY:
            JSON_ENCODE_ARRAY_START(encoder);
            for (i = 0; i < jv->jv_len; i++) {
                rc = json_encode_value(encoder, jv->jv_val.composite.values[i]);
                if (rc != 0) {
                    goto err;
                }
                if (i != jv->jv_len - 1) {
                    encoder->je_write(encoder->je_arg, ",", sizeof(",")-1);
                }
            }
            JSON_ENCODE_ARRAY_END(encoder);
            break;
        case JSON_VALUE_TYPE_OBJECT:
            JSON_ENCODE_OBJECT_START(encoder);
            for (i = 0; i < jv->jv_len; i++) {
                rc = json_encode_object_entry(encoder,
                        jv->jv_val.composite.keys[i],
                        jv->jv_val.composite.values[i]);
                if (rc != 0) {
                    goto err;
                }
            }
            JSON_ENCODE_OBJECT_END(encoder);
            break;
        default:
            rc = -1;
            goto err;
    }


    return (0);
err:
    return (rc);
}

int
json_encode_object_key(struct json_encoder *encoder, char *key)
{
    if (encoder->je_wr_commas) {
        encoder->je_write(encoder->je_arg, ",", sizeof(",")-1);
        encoder->je_wr_commas = 0;
    }

    /* Write the key entry */
    encoder->je_write(encoder->je_arg, "\"", sizeof("\"")-1);
    encoder->je_write(encoder->je_arg, key, strlen(key));
    encoder->je_write(encoder->je_arg, "\": ", sizeof("\": ")-1);

    return (0);
}

int
json_encode_object_entry(struct json_encoder *encoder, char *key,
        struct json_value *val)
{
    int rc;

    if (encoder->je_wr_commas) {
        encoder->je_write(encoder->je_arg, ",", sizeof(",")-1);
        encoder->je_wr_commas = 0;
    }
    /* Write the key entry */
    encoder->je_write(encoder->je_arg, "\"", sizeof("\"")-1);
    encoder->je_write(encoder->je_arg, key, strlen(key));
    encoder->je_write(encoder->je_arg, "\": ", sizeof("\": ")-1);

    rc = json_encode_value(encoder, val);
    if (rc != 0) {
        goto err;
    }
    encoder->je_wr_commas = 1;

    return (0);
err:
    return (rc);
}

int
json_encode_object_finish(struct json_encoder *encoder)
{
    JSON_ENCODE_OBJECT_END(encoder);
    /* Useful in case of nested objects. */
    encoder->je_wr_commas = 1;

    return (0);
}

int
json_encode_array_name(struct json_encoder *encoder, char *name)
{
    return json_encode_object_key(encoder, name);
}

int
json_encode_array_start(struct json_encoder *encoder)
{
    JSON_ENCODE_ARRAY_START(encoder);
    encoder->je_wr_commas = 0;

    return (0);
}

int
json_encode_array_value(struct json_encoder *encoder, struct json_value *jv)
{
    int rc;

    if (encoder->je_wr_commas) {
        encoder->je_write(encoder->je_arg, ",", sizeof(",")-1);
        encoder->je_wr_commas = 0;
    }

    rc = json_encode_value(encoder, jv);
    if (rc != 0) {
        goto err;
    }
    encoder->je_wr_commas = 1;

    return (0);
err:
    return (rc);
}


int
json_encode_array_finish(struct json_encoder *encoder)
{
    encoder->je_wr_commas = 1;
    JSON_ENCODE_ARRAY_END(encoder);

    return (0);
}
