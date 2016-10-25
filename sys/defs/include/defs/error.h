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

#ifndef H_DEFS_ERROR_
#define H_DEFS_ERROR_

#ifdef __cplusplus
extern "C" {
#endif

#define EOK      (0)
#define ENOMEM   (-1)
#define EINVAL   (-2)
#define ETIMEOUT (-3)
#define ENOENT   (-4)
#define EIO      (-5)
#define EAGAIN   (-6)
#define EACCES   (-7)
#define EBUSY    (-8)
#define ENODEV   (-9)
#define ERANGE   (-10)

#define E_PERUSER (-65535)

#ifdef __cplusplus
}
#endif


#endif /* H_DEFS_ERROR_ */
