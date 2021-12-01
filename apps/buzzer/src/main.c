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

#include "sysinit/sysinit.h"
#include "os/os.h"

#include "buzzer/buzzer.h"

int count = 0;

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    while (1) {

        /* quarter of second */
        os_time_delay(OS_TICKS_PER_SEC / 4);

        /* each quarter of second */
        switch (count++ % 4) {

        case 0:
            /* activate buzzer tone at 2 KHz frequency */
            buzzer_tone_on(2000);
            break;

        case 1:
            /* disable buzzer */
            buzzer_tone_off();
            break;

        default:
            /* empty */
            break;
        }
    }

    assert(0);
}

