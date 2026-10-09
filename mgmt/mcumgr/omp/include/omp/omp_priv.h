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

#ifndef H_OMP_PRIV
#define H_OMP_PRIV

#include "mgmt/mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

int omp_encode_mgmt_hdr(struct CborEncoder *enc, struct mgmt_hdr hdr);
int omp_send_err_rsp(struct CborEncoder *enc,
                     const struct mgmt_hdr *hdr,
                     int mgmt_status);
int omp_process_mgmt_hdr(struct mgmt_hdr *req_hdr,
                         struct mgmt_hdr *rsp_hdr,
                         struct mgmt_ctxt *ctxt);

#ifdef __cplusplus
}
#endif

#endif /* H_OMP_IMPL */
