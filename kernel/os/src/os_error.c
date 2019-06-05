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

#include "defs/error.h"
#include "os/os_error.h"

int
os_error_to_sys(os_error_t os_error)
{
    switch (os_error) {
        case OS_OK:             return SYS_EOK;
        case OS_ENOMEM:         return SYS_ENOMEM; 
        case OS_EINVAL:         return SYS_EINVAL; 
        case OS_INVALID_PARM:   return SYS_EINVAL; 
        case OS_TIMEOUT:        return SYS_ETIMEOUT; 
        case OS_ENOENT:         return SYS_ENOENT; 
        case OS_EBUSY:          return SYS_EBUSY; 
        default:                return SYS_EUNKNOWN;
    }
}
