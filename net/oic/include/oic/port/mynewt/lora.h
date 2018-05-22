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

#ifndef __OIC_MYNEWT_LORA_H_
#define __OIC_MYNEWT_LORA_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * oc_endpoint for LORA.
 */
struct oc_endpoint_lora {
    struct oc_ep_hdr ep;
    uint8_t port;
};

extern uint8_t oc_lora_transport_id;

#ifdef __cplusplus
}
#endif

#endif /* __OIC_MYNEWT_LORA_H_ */
