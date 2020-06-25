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

#ifndef __IP_PRIV_H__
#define __IP_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

struct mn_itf;
struct mn_itf_addr;
int lwip_itf_getnext(struct mn_itf *mi);
int lwip_itf_addr_getnext(struct mn_itf *mi, struct mn_itf_addr *mia);

int lwip_socket_init(void);
void lwip_cli_init(void);

int lwip_err_to_mn_err(int rc);

#ifdef __cplusplus
}
#endif

#endif /* __IP_PRIV_H__ */
