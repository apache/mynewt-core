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

#ifndef H_TINYCBOR_CBOR_READER_PRIV_
#define H_TINYCBOR_CBOR_READER_PRIV_

#include <tinycbor/cbor.h>

/*
 * Copies (or calculates length of if buffer is NULL) string from value parsed
 * with Mynewt CBOR reader, regardless of string layout in source data.
 */
CborError cbor_reader_copy_string(const CborValue *value, void *buffer,
                                  size_t *buflen, CborValue *next);

static inline bool
cbor_reader_is_used(const CborValue *value)
{
    return value->parser->flags & CborParserFlag_ExternalSource;
}

#endif
