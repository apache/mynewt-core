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

#include "console/console.h"
#include "coremark/coremark_api.h"

extern uint32_t SystemCoreClock;

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    printf("Coremark running on %s at %lu MHz\n\n", MYNEWT_VAL(BSP_NAME), SystemCoreClock / 1000000L);

    coremark_run();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
}
