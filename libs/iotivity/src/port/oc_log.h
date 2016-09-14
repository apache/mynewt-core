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

#define PRINT(...) printf(__VA_ARGS__)

#define PRINTipaddr(endpoint)                                                  \
  PRINT(                                                                       \
    "[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%"    \
    "02x]:%d",                                                                 \
    ((endpoint).ipv6_addr.address)[0], ((endpoint).ipv6_addr.address)[1],      \
    ((endpoint).ipv6_addr.address)[2], ((endpoint).ipv6_addr.address)[3],      \
    ((endpoint).ipv6_addr.address)[4], ((endpoint).ipv6_addr.address)[5],      \
    ((endpoint).ipv6_addr.address)[6], ((endpoint).ipv6_addr.address)[7],      \
    ((endpoint).ipv6_addr.address)[8], ((endpoint).ipv6_addr.address)[9],      \
    ((endpoint).ipv6_addr.address)[10], ((endpoint).ipv6_addr.address)[11],    \
    ((endpoint).ipv6_addr.address)[12], ((endpoint).ipv6_addr.address)[13],    \
    ((endpoint).ipv6_addr.address)[14], ((endpoint).ipv6_addr.address)[15],    \
    (endpoint).ipv6_addr.port)

#if DEBUG
#define LOG(...) PRINT(__VA_ARGS__)
#define LOGipaddr(endpoint) PRINTipaddr(endpoint)
#else
#define LOG(...)
#define LOGipaddr(endpoint)
#endif

#endif /* OC_LOG_H */
