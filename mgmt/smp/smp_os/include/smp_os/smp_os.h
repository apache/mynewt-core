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

#ifndef _SMP_OS_H_
#define _SMP_OS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Id's for OS group commands
 */
#define SMP_ID_ECHO            0
#define SMP_ID_CONS_ECHO_CTRL  1
#define SMP_ID_TASKSTATS       2
#define SMP_ID_MPSTATS         3
#define SMP_ID_DATETIME_STR    4
#define SMP_ID_RESET           5

int mgmt_os_groups_register(void);

#ifdef __cplusplus
}
#endif

#endif /* _SMP_OS_H_ */