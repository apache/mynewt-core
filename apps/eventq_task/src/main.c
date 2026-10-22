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

#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "static_task/static_task.h"

/*
 * Defines task blink_task, its stack and event queue blink_evq.
 * The task is created by sysinit at STATIC_TASK_SYSINIT_STAGE and
 * processes events from blink_evq forever; no task handler or
 * os_task_init() call is needed.
 */
STATIC_EVENTQ_TASK_DEFINE(blink, MYNEWT_VAL(EVENTQ_TASK_PRIO),
                          MYNEWT_VAL(EVENTQ_TASK_STACK_SIZE));

static struct os_callout blink_callout;

static void
hello_ev_cb(struct os_event *ev)
{
    console_printf("%s from task %s\n", (const char *)ev->ev_arg,
                   os_sched_get_current_task()->t_name);
}

/* Put on blink_evq during sysinit, before blink task exists */
static struct os_event early_ev = {
    .ev_cb = hello_ev_cb,
    .ev_arg = "Event queued during sysinit",
};

/* Put on blink_evq from main task */
static struct os_event main_ev = {
    .ev_cb = hello_ev_cb,
    .ev_arg = "Event queued by main",
};

static void
blink_timer_cb(struct os_event *ev)
{
    /* Callout events are delivered to blink_evq, this runs in blink task */
    hal_gpio_toggle(LED_BLINK_PIN);
    os_callout_reset(&blink_callout, OS_TICKS_PER_SEC);
}

void
eventq_task_app_init(void)
{
    hal_gpio_init_out(LED_BLINK_PIN, 1);

    /* blink_evq is statically initialized, it can be used right away */
    os_callout_init(&blink_callout, &blink_evq, blink_timer_cb, NULL);
    os_callout_reset(&blink_callout, OS_TICKS_PER_SEC);

    os_eventq_put(&blink_evq, &early_ev);
}

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    os_eventq_put(&blink_evq, &main_ev);

    os_eventq_loop(os_eventq_dflt_get());
}
