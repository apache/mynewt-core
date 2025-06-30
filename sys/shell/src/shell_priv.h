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

#ifndef __SHELL_PRIV_H_
#define __SHELL_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "streamer/streamer.h"
#include "shell/shell.h"
struct CborEncoder;
 
#if MYNEWT_VAL(SHELL_BRIDGE)
/**
 * Streams CBOR text strings to its encoder.
 */
struct shell_bridge_streamer {
    struct streamer streamer;
    struct CborEncoder *str_encoder;
};
#endif

#if MYNEWT_VAL(SHELL_MGMT)
#define SHELL_NLIP_PKT_START1 (6)
#define SHELL_NLIP_PKT_START2 (9)
#define SHELL_NLIP_DATA_START1 (4)
#define SHELL_NLIP_DATA_START2 (20)

int shell_nlip_process(char *data, int len);
void shell_nlip_init(void);
void shell_nlip_clear_pkt(void);
#endif

void shell_prompt_register(void);

#if MYNEWT_VAL(SHELL_BRIDGE)
void shell_bridge_streamer_new(struct shell_bridge_streamer *sbs,
                               struct CborEncoder *str_encoder);
int shell_bridge_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_PRIV_H_ */
