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

#ifndef _FLASH_LOADER_H
#define _FLASH_LOADER_H

/*
 * Flash loader state.
 */
#define FL_WAITING              1
#define FL_EXECUTING            2

/*
 * Flash loader commands
 */
#define FL_CMD_PING             1
#define FL_CMD_LOAD             2
#define FL_CMD_ERASE            3
#define FL_CMD_VERIFY           4
#define FL_CMD_LOAD_VERIFY      5
#define FL_CMD_DUMP             6

/*
 * Return codes
 */
#define FL_RC_OK                1
#define FL_RC_FLASH_ERR         2
#define FL_RC_VERIFY_ERR        3
#define FL_RC_UNKNOWN_CMD_ERR   4
#define FL_RC_ARG_ERR           5
#endif
