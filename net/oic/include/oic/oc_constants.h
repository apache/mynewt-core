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

#ifndef OC_CONSTANTS_H
#define OC_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* OCF standard resource interfaces */
#define OC_NUM_STD_INTERFACES (7)
#define OC_RSRVD_IF_BASELINE "oic.if.baseline"
#define OC_BASELINE_IF_LEN (15)
#define OC_RSRVD_IF_LL "oic.if.ll"
#define OC_LL_IF_LEN (9)
#define OC_RSRVD_IF_B "oic.if.b"
#define OC_B_IF_LEN (8)
#define OC_RSRVD_IF_R "oic.if.r"
#define OC_R_IF_LEN (8)
#define OC_RSRVD_IF_RW "oic.if.rw"
#define OC_RW_IF_LEN (9)
#define OC_RSRVD_IF_A "oic.if.a"
#define OC_A_IF_LEN (8)
#define OC_RSRVD_IF_S "oic.if.s"
#define OC_S_IF_LEN (8)

/* OCF Core resource URIs */
#define OC_RSRVD_WELL_KNOWN_URI "/oic/res"
#define OC_MULTICAST_DISCOVERY_URI "/oic/res"
#define OC_RSRVD_DEVICE_URI "/oic/d"
#define OC_RSRVD_PLATFORM_URI "/oic/p"

#ifdef __cplusplus
}
#endif

#endif /* OC_CONSTANTS_H */
