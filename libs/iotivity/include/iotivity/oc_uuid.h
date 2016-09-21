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

#ifndef OC_UUID_H
#define OC_UUID_H

#include <stdint.h>

typedef struct
{
  uint8_t id[16];
} oc_uuid_t;

void oc_str_to_uuid(const char *str, oc_uuid_t *uuid);
void oc_uuid_to_str(const oc_uuid_t *uuid, char *buffer, int buflen);
void oc_gen_uuid(oc_uuid_t *uuid);

#endif /* OC_UUID_H */
