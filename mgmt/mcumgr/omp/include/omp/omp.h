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

/**
 * @file
 * @brief OMP - OIC Management Protocol.
 *
 * OMP is an OIC implementation of SMP, a basic protocol that sits on top of
 * the mgmt layer. SMP requests and responses have the following format:
 *
 *     [Offset 0]: Mgmt header
 *     [Offset 8]: CBOR map of command-specific key-value pairs.
 *
 * SMP request packets may contain multiple concatenated requests.  Each
 * request must start at an offset that is a multiple of 4, so padding should
 * be inserted between requests as necessary.  Requests are processed
 * sequentially from the start of the packet to the end.  Each response is sent
 * individually in its own packet.  If a request elicits an error response,
 * processing of the packet is aborted.
 */

#ifndef H_OMP_
#define H_OMP_

#include "mgmt/mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct omp_streamer;
struct mgmt_hdr;

/**
 * @brief Transmits an OMP response.
 *
 * @param stmr                  The OMP Streamer struct.
 * @param retval                The return value for the request.
 * @param arg                   Optional streamer argument.
 */
typedef void omp_tx_rsp_fn(struct omp_streamer *stmr, int retval, void* arg);

/**
 * @brief Decodes, encodes, and transmits OMP requests and responses.
 *        Holds the callback pointer and the response CBOR encoder provided by
 *        underlying OIC implementation.
 */
struct omp_streamer {
    struct mgmt_streamer mgmt_stmr;
    struct CborEncoder *rsp_encoder;
    omp_tx_rsp_fn *tx_rsp_cb;
};

/**
 * @brief Stores the streamer, management context and request.
 */
struct omp_state {
    struct omp_streamer omp_stmr;
    struct mgmt_ctxt *m_ctxt;
    void *request;
};


/**
 * @brief Processes a single OMP request packet and sends all corresponding
 *        responses.
 *
 * Processes all OMP requests in an incoming packet.  Requests are processed
 * sequentially from the start of the packet to the end.  Each response is sent
 * individually in its own packet.  If a request elicits an error response,
 * processing of the packet is aborted.  This function consumes the supplied
 * request buffer regardless of the outcome.//encoder
 *
 * @param omgr_st               The OMP State struct
 * @param req                   The request packet to process.
 *
 * @return                      0 on success, MGMT_ERR_[...] code on failure.
 */
int omp_impl_process_request_packet(struct omp_state *omgr_st, void *req);

/**
 * @brief Read the management header out from a cbor value
 *
 * @param cv      Ptr to CoberValue
 * @param out_hdr Ptr to management header to be filled in
 */ 
int omp_read_hdr(struct CborValue *cv, struct mgmt_hdr *out_hdr);

#ifdef __cplusplus
}
#endif

#endif /* H_OMP_ */
