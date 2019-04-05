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

#ifndef H_OICMGR_
#define H_OICMGR_

#include "oic/oc_ri.h"
#include "mgmt/mgmt.h"

/**
 * Process an oicmgr request.  On completion, an oicmgr response is sent back
 * to the client.
 *
 * @param req                   The oicmgr request to process.
 * @param mask                  The oicmgr mask supported by the CoAP request
 *                                  handler.
 *
 * @return                      0 on success;
 *                              nonzero on failure (not consistent code sets).
 */
int omgr_oic_process_put(oc_request_t *req, oc_interface_mask_t mask);

/**
 * Parses an oicmgr request and copies out the NMP header.
 *
 * @param req                   The oicmgr request to parse.
 * @param out_hdr               On success, the extracted NMP header gets
 *                                  written here.
 *
 * @return                      0 on success;
 *                              MGMT_ERR_EINVAL on parse failure.
 */
int omgr_extract_req_hdr(oc_request_t *req, struct nmgr_hdr *out_hdr);

#endif
