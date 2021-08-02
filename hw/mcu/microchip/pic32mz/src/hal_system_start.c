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

#include <assert.h>

#define PHY_TO_VIRT(address)   (unsigned int)((int)(address) | 0x80000000)

void
hal_system_start(void *img_start)
{
    void (*app_reset)(void) = (void (*)(void))PHY_TO_VIRT(img_start);

    app_reset();
    while (1);
}

void
hal_system_restart(void *img_start)
{
    hal_system_restart(img_start);
}

