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

#ifndef _HW_DRIVERS_I2S_STREAM_H
#define _HW_DRIVERS_I2S_STREAM_H

#include <stdint.h>
#include <i2s/i2s.h>

#ifdef __cplusplus
extern "C" {
#endif

struct i2s_out_stream {
    struct out_stream ostream;
    struct i2s *i2s;
    struct i2s_sample_buffer *buffer;
};

#define I2S_OUT_STREAM_DEF(var) \
    extern const struct out_stream_vft i2s_out_stream_vft; \
    struct i2s_out_stream var = { \
        OSTREAM_INIT(i2s_out_stream, ostream), \
    };

#ifdef __cplusplus
}
#endif

#endif /* _HW_DRIVERS_I2S_STREAM_H */
