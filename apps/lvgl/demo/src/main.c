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

#include <assert.h>

#include <sysinit/sysinit.h>
#include <os/os.h>
#include <demos/benchmark/lv_demo_benchmark.h>
#include <demos/music/lv_demo_music.h>
#include <demos/stress/lv_demo_stress.h>
#include <demos/widgets/lv_demo_widgets.h>
#include <examples/widgets/lv_example_widgets.h>
#include "hal/hal_gpio.h"
#include <console/console.h>

int
main(int argc, char **argv)
{
    sysinit();

    if (MYNEWT_VAL_CHOICE(LVGL_DEMO, benchmark)) {
        lv_demo_benchmark();
    } else if (MYNEWT_VAL_CHOICE(LVGL_DEMO, keypad_encoder)) {
        lv_demo_keypad_encoder();
    } else if (MYNEWT_VAL_CHOICE(LVGL_DEMO, music)) {
        lv_demo_music();
    } else if (MYNEWT_VAL_CHOICE(LVGL_DEMO, stress)) {
        lv_demo_stress();
    } else if (MYNEWT_VAL_CHOICE(LVGL_DEMO, widgets)) {
        lv_demo_widgets();
    }

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    return 0;
}

