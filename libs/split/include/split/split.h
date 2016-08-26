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

#ifndef _SPLIT_H__
#define _SPLIT_H__

#define SPLIT_NMGR_OP_SPLIT 0

typedef enum splitMode_e {
    SPLIT_NONE,
    SPLIT_TEST,
    SPLIT_RUN,
} splitMode_t;


typedef enum splitStatus_e {
    SPLIT_INVALID,
    SPLIT_NOT_MATCHING,
    SPLIT_MATCHING,
}splitStatus_t;

/*
  * Initializes the split application library */
void
split_app_init(void);

/**
  * checks the split application state.
  * If the application is configured to be run (and valid)
  * returns zero and puts the entry data into entry. NOTE:
  * Entry data is not a function pointer, but a pointer
  * suitable to call system_start
  *
  * If toBoot is true, also performs the necessary steps
  * to prepare to boot.  An application may set toBoot to
  * false and call this function to check whether the split
  * application is bootable */
int
split_app_go(void **entry, int toBoot);

#endif /* _SPLIT_H__ */
