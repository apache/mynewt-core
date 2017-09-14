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

#ifndef _MESH_PTS_
#define _MESH_PTS_

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(BLE_MESH_PTS)

#include "glue.h"

int pts_mesh_net_send_msg(u8_t ttl, u16_t app_idx, u16_t src_addr,
                          u16_t dst_addr, u8_t *buf, u16_t len);

#endif /* MYNEWT_VAL(BLE_MESH_PTS) */

#endif
