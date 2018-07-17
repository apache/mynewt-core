/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef OC_LOG_H
#define OC_LOG_H

#include <stdio.h>
#include <modlog/modlog.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * XXX, this should not be needed.
 * Figure out why logging takes so much space even with LOG_LEVEL set to 4
 */
#if MYNEWT_VAL(OC_LOGGING)

#define OC_LOG(lvl_, ...) MODLOG_ ## lvl_(LOG_MODULE_IOTIVITY, __VA_ARGS__)

struct oc_endpoint;
void oc_log_endpoint(uint8_t lvl, struct oc_endpoint *);
void oc_log_bytes(uint8_t lvl, void *addr, int len, int print_char);
struct os_mbuf;
void oc_log_bytes_mbuf(uint8_t lvl, struct os_mbuf *, int off, int len,
                       int print_char);

#define OC_LOG_ENDPOINT(lvl, ep)                                        \
    do {                                                                \
        if (MYNEWT_VAL(LOG_LEVEL) <= (lvl)) {                           \
            oc_log_endpoint(lvl, ep);                                   \
        }                                                               \
    } while(0)

#define OC_LOG_STR(lvl, addr, len)                                      \
    do {                                                                \
        if (MYNEWT_VAL(LOG_LEVEL) <= (lvl)) {                           \
            oc_log_bytes(lvl, addr, len, 1);                            \
        }                                                               \
    } while(0)

#define OC_LOG_STR_MBUF(lvl, m, off, len)                               \
    do {                                                                \
        if (MYNEWT_VAL(LOG_LEVEL) <= (lvl)) {                           \
            oc_log_bytes_mbuf(lvl, m, off, len, 1);                     \
        }                                                               \
    } while(0)

#define OC_LOG_HEX(lvl, addr, len)                                      \
    do {                                                                \
        if (MYNEWT_VAL(LOG_LEVEL) <= (lvl)) {                           \
            oc_log_bytes(lvl, addr, len, 0);                            \
        }                                                               \
    } while(0)

#define OC_LOG_HEX_MBUF(lvl, m, off, len)                               \
    do {                                                                \
        if (MYNEWT_VAL(LOG_LEVEL) <= (lvl)) {                           \
            oc_log_bytes_mbuf(lvl, m, off, len, 0);                     \
        }                                                               \
    } while(0)

#else

#define OC_LOG(lvl_, ...)
#define OC_LOG_ENDPOINT(...)
#define OC_LOG_STR(...)
#define OC_LOG_STR_MBUF(...)
#define OC_LOG_HEX(...)
#define OC_LOG_HEX_MBUF(...)

#endif

#ifdef __cplusplus
}
#endif

#endif /* OC_LOG_H */
