/**
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

#ifndef CBOR_CNT_WRITER_H
#define CBOR_CNT_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

    /* use this count writer if you want to try out a cbor encoding to see
     * how long it would be (before allocating memory). This replaced the
     * code in tinycbor.h that would try to do this once the encoding failed
     * in a buffer.  Its much easier to understand this way (for me)
     */

struct CborCntWriter {
    int bytes_written;
};

inline void
cbor_cnt_writer_init(struct CborCntWriter *cb) {
    cb->bytes_written = 0;
}

inline int
cbor_cnt_writer(void *arg, char *data, int len) {
    struct CborCntWriter *cb = (struct CborCntWriter *) arg;
    cb->bytes_written += len;
    return CborNoError;
}

inline int
cbor_cnt_writer_length(struct CborCntWriter *cb) {
    return cb->bytes_written;
}

#ifdef __cplusplus
}
#endif

#endif /* CBOR_CNT_WRITER_H */

