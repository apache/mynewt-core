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

#ifndef OC_STORAGE_H
#define OC_STORAGE_H

#include <stddef.h>
#include <stdint.h>

int oc_storage_config(const char *store);
long oc_storage_read(const char *store, uint8_t *buf, size_t size);
long oc_storage_write(const char *store, uint8_t *buf, size_t size);

#endif /* OC_STORAGE_H */
