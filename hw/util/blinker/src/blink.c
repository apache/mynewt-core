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
#include <syscfg/syscfg.h>

#include "blinker/blink.h"

#define BLINK_SCHEDULED_DISABLED                0

#define BLINK_FLG_FIRST                         0x01

#define BLINK_TIME_FOREVER                      0xffff

static inline void blink_schedule_next_sequence(blink_t *blink);

static struct os_eventq *blink_evq = NULL;
    
/*
 * Schedule the next on/off for a blink code.
 *
 * @param blink                 blink handler
 * @param on                    pointer on time for 'on' state
 * @param off                   pointer on time for 'off' state
 *
 * @return if the sequence is to be continued
 */
static bool
blink_onoff_code(blink_t *blink, uint16_t *on, uint16_t *off)
{
    uint8_t count;
    blink_code_t code;

    /* Data */
    code = (blink_code_t) blink->running.data;

    /* Special case */
    switch(code) {
    case BLINK_ON:
        *on  = BLINK_TIME_FOREVER;
        return blink->running.step++ <= 0;

    case BLINK_OFF:
        *off = BLINK_TIME_FOREVER;
        return blink->running.step++ <= 0;
    }

    /* Get count */
    count = BLINK_GET_COUNT(code);
    
    /* Check if aborting blinking sequence is requested */
    if (blink->next.scheduled) {
        if ((count == 0                  ) ||     /* Continuous blink */
            (count == blink->running.step) ||     /* End of group     */
            (blink->next.scheduled == BLINK_SCHEDULED_WAIT_BLINK)) {
            return false;
        }
    }
    
    /* Off state */
    if (count == 0) {                                   /* Continuous blink */
        *off = BLINK_GET_DELAY(code);
    } else if (count == blink->running.step++) {        /* Streak ended     */
        if ((*off = BLINK_GET_WAIT(code))) {
            blink->running.step = 1;
        } else {
            return false;
        }
    } else {                                            /* Streak           */
        *off = BLINK_GET_DELAY(code);
    }
    
    /* On state */
    *on = BLINK_GET_LENGTH(code);

    return true;
}

/*
 * Schedule the next on/off for a morse
 *
 * @param blink                 blink handler
 * @param on                    pointer on time for 'on' state
 * @param off                   pointer on time for 'off' state
 *
 * @return if the sequence is to be continued
 */
static bool
blink_onoff_dotdash(blink_t *blink, uint16_t *on, uint16_t *off)
{
    char *data;

    /* Data */
    data = (char *) blink->running.data;

    /* Check if aborting blinking sequence is requested */
    if (blink->next.scheduled == BLINK_SCHEDULED_WAIT_BLINK) {
        return false;
    }

    /* Dots and Dashes */
 next:
    switch(data[blink->running.step++]) {
    case '.' : *on = 1; *off = 1; break;
    case '-' : *on = 3; *off = 1; break;
    case '\0': return false;
    default:
        goto next;
    }

    return true;
}

/*
 * Mark the blink handle as stopped (ie: not running).
 *
 * @param blink                 blink handler
 */
static inline void
blink_mark_stopped(blink_t *blink, bool forced)
{
    blink->running.onoff = NULL;
    blink->last_time     = forced ? OS_WAIT_FOREVER : os_time_get();
}

/*
 * Schedule callouts for next on+off.
 *
 * @param blink                 blink handler
 */
static void
blink_schedule_next_onoff(blink_t *blink) 
{
    uint16_t b_on, b_off;
    
    /* Check for next blink */
    if (blink->running.onoff(blink, &b_on, &b_off)) {
        /* Infinite on */
        if (b_on  == BLINK_TIME_FOREVER) {
            blink->c_state = true;
            goto infinite;
            
        /* Infinite off */
        } else if (b_off == BLINK_TIME_FOREVER) {
            blink->c_state = false;
        infinite:
            blink->c_next  = 0;
            os_callout_reset(&blink->onoff_callout, 0);

        /* Normal case */
        } else {
            /* Compute delay for the first callout 
             */
            os_time_t delay;

            /* Special handling for first run */
            if (blink->running.flags & BLINK_FLG_FIRST) {
                blink->running.flags &= ~BLINK_FLG_FIRST;
                /* Take into account separator time */
                if (blink->last_time != OS_WAIT_FOREVER) {
                    delay = os_time_get() - blink->last_time;
                    if (delay < (blink->separator * blink->time_unit)) {
                        delay = (blink->separator * blink->time_unit) - delay;
                        goto callouts;
                    }
                }
                /* Directly move to 'on' state */
                delay = 0;
            } else {
                delay = b_off * blink->time_unit;
            }

            /* Set callouts for on+off 
             */
        callouts:
            blink->c_state = true;
            blink->c_next  = b_on * blink->time_unit;
            os_callout_reset(&blink->onoff_callout, delay);
        }
    /* No more blink */
    } else {
        /* Mark blink as stopped */
        blink_mark_stopped(blink, false);

        /* Check if a new sequence is pending 
         *   (if onoff=NULL is scheduled, that's the stop sequence)
         */
        if (blink->next.scheduled && blink->next.onoff)
            blink_schedule_next_sequence(blink);
    }
}

/*
 * Schedule next blinking sequence.
 */
static inline void
blink_schedule_next_sequence(blink_t *blink)
{
    assert(blink->running.onoff == NULL);
    assert(blink->next.onoff    != NULL);

    /* Move from next to running */
    blink->running.onoff  = blink->next.onoff;
    blink->running.data   = blink->next.data;
    blink->running.step   = 0;

    /* Reset next state */
    blink->next.onoff     = NULL;
    blink->next.scheduled = BLINK_SCHEDULED_DISABLED;

    /* Schedule first on+off */
    blink->running.flags |= BLINK_FLG_FIRST;
    blink_schedule_next_onoff(blink);
}

/*
 * Process 'on'/'off' blink event.
 *
 * @param ev                    generated 'on'/'off' event
 */
static void
blink_onoff_callout_handler(struct os_event *ev)
{
    blink_t *blink;
    blink = (blink_t *)ev->ev_arg;
    blink->set_state(blink->c_state);

    os_mutex_pend(&blink->mutex, OS_WAIT_FOREVER);
    if (blink->c_next) {
        if (blink->c_state) {
            blink->c_state = false;
            os_callout_reset(&blink->onoff_callout, blink->c_next);
        } else {
            blink_schedule_next_onoff(blink);
        }
    }
    os_mutex_release(&blink->mutex);
}

void
blink_init(blink_t *blink)
{
    struct os_eventq *eventq;

    /* Ensure we got a reasonable default value for time_unit */
    if (blink->time_unit == 0) {
        blink->time_unit = MYNEWT_VAL(BLINK_TIME_UNIT);
    }

    /* Initialize mutex */
    os_mutex_init(&blink->mutex);

    /* Initialize callout */
    eventq = blink_evq;
    if (eventq == NULL) {
        eventq = os_eventq_dflt_get();
    }
    os_callout_init(&blink->onoff_callout, eventq,
                    blink_onoff_callout_handler, blink);

    /* Last time */
    blink->last_time = OS_WAIT_FOREVER;
}

/**
 * Schedule blinking sequence.
 *
 * @param blink                 blink handler
 * @param onoff                 on/off function
 * @param data                  description for the on/off sequence
 * @param scheduled             when to start the blinking sequence:
 *                                o BLINK_SCHEDULED_IMMEDIATE
 *                                o BLINK_SCHEDULED_WAIT_BLINK
 *                                o BLINK_SCHEDULED_WAIT_SEQUENCE
 */
static void
blink_schedule(blink_t *blink, blink_onoff_t onoff, void *data, int scheduled)
{
    os_mutex_pend(&blink->mutex, OS_WAIT_FOREVER);

    /* Set Next */
    blink->next.onoff     = onoff;
    blink->next.data      = data;
    blink->next.scheduled = scheduled;
    
    /* If immediate, force a stop */
    if (scheduled == BLINK_SCHEDULED_IMMEDIATE) {
        blink->c_state = false;
        blink->c_next  = 0;
        os_callout_reset(&blink->onoff_callout, 0);
        blink_mark_stopped(blink, true);
    }

    /* If no blink is running, start it now.
     * Otherwise it is pending and will be started when apropriate
     */
    if ((blink->running.onoff == NULL) &&
        (blink->next.onoff    != NULL)) {
        blink_schedule_next_sequence(blink);
    }

    os_mutex_release(&blink->mutex);
}

void
blink_evq_set(struct os_eventq *evq)
{
    blink_evq = evq;
}

void
blink_code(blink_t *blink, blink_code_t code, int scheduled)
{
    assert(sizeof(blink_code_t) <= sizeof(void*));
    assert(scheduled != BLINK_SCHEDULED_DISABLED);

    blink_schedule(blink, blink_onoff_code, (void*)code, scheduled);
}

void
blink_dotdash(blink_t *blink, char *dotdash, int scheduled)
{
    assert(scheduled != BLINK_SCHEDULED_DISABLED);

    blink_schedule(blink, blink_onoff_dotdash, dotdash, scheduled);
}

void
blink_stop(blink_t *blink, int scheduled)
{
    assert(scheduled != BLINK_SCHEDULED_DISABLED);
    
    blink_schedule(blink, NULL, NULL, scheduled);
}
