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

#ifndef OC_ASSERT_H
#define OC_ASSERT_H

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define oc_abort(msg)                                                   \
    do {                                                                \
      OC_LOG(ERROR, "error: %s\nAbort.\n", msg);                         \
      assert(0);                                                        \
  } while (0)

#define oc_assert(cond) assert(cond)

#ifdef __cplusplus
}
#endif

#endif /* OC_ASSERT_H */
