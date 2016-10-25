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

#ifndef H_BASELIBC_UNITTESTS_
#define H_BASELIBC_UNITTESTS_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMMENT(x) printf("\n----" x "----\n");
#define STR(x) #x
#define STR2(x) STR(x)
#define TEST(x) \
    if (!(x)) { \
        fprintf(stderr, "\033[31;1mFAILED:\033[22;39m " __FILE__ ":" STR2(__LINE__) " " #x "\n"); \
        status = 1; \
    } else { \
        printf("\033[32;1mOK:\033[22;39m " #x "\n"); \
    }

#ifdef __cplusplus
}
#endif

#endif
