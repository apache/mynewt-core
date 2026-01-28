/**
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

#ifndef MTEST_PRIV_H
#define MTEST_PRIV_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct case_context {
    const char *name;
    uint8_t failed;
};

struct suite_context {
    const char *name;
    int tests_run;
    int tests_failed;
    int tests_passed;
    uint8_t aborted;
};

#ifdef __cplusplus
}
#endif

#endif
