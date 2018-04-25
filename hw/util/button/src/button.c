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
#include "button/button.h"

#define _BUTTON_FSM_INIT                        0
#define _BUTTON_FSM_PRESSED                     1
#define _BUTTON_FSM_WAIT_DBLPRESSED             2
#define _BUTTON_FSM_DBLPRESSED                  3
#define _BUTTON_FSM_HOLD_OR_REPEAT              4

#define _BUTTON_RELEASED                        0
#define _BUTTON_PRESSED                         1
#define _BUTTON_TIMEOUT                         2

struct button_event {
    struct os_event os_event;
    uint8_t type;
    uint8_t flags;
};

static struct os_eventq *button_internal_evq = NULL;
static struct os_eventq *button_callback_default_evq = NULL;

#define BUTTON_POST_STATE_EVENT(button, flags)  \
    button_post_event(button, BUTTON_STATE_CHANGED, flags)

#define BUTTON_POST_ACTION_EVENT(button, flags) \
    button_post_event(button, BUTTON_ACTION, flags)

static struct button_event button_event[MYNEWT_VAL(BUTTON_EVENT_MAX)] = { 0 };
static button_callback_t button_callback = NULL;

static struct button_event *
button_alloc_event(void)
{
    int i;

    for (i = 0 ; i < MYNEWT_VAL(BUTTON_EVENT_MAX) ; i++) {
        if (!OS_EVENT_QUEUED(&button_event[i].os_event)) {
            return &button_event[i];
        }
    }

    return NULL;
}

/**
 * Process generated event, using the user defined callback.
 *
 * @param ev generated event
 */
static void
button_event_handler(struct os_event *ev)
{
    struct button_event *b_ev;
    button_t *button;

    b_ev   = (struct button_event *)ev;
    button = (button_t *)ev->ev_arg;
    button_callback(button->id, b_ev->type, b_ev->flags);
}

/**
 * Post an event to the default queue.
 * 
 * @param button        button generating the event
 * @param type          type of event (state or action)
 * @param flags         event description (BUTTON_FLG_*)
 *
 * @return              -1 unable to post event (not enough buffer)
 *                       0 event posted
 *                       1 event not posted due to filtering
 */
static int
button_post_event(button_t *button, uint8_t type, uint8_t flags)
{
    struct button_event *evt;

#if MYNEWT_VAL(BUTTON_USE_FILTERING)
    if (button->filter.enabled) {
        uint8_t filter = 0xff;
        switch(type) {
#if MYNEWT_VAL(BUTTON_EMIT_STATE_CHANGED)
        case BUTTON_STATE_CHANGED:
            filter = button->filter.state;
            break;
#endif
#if MYNEWT_VAL(BUTTON_EMIT_ACTION)
        case BUTTON_ACTION:
            filter = button->filter.action;
            break;
#endif
        }
        if (~filter & flags) {
            return 1;
        }
    }
#endif

    evt = button_alloc_event();
    if (evt) {  
        evt->os_event.ev_cb  = button_event_handler;
        evt->os_event.ev_arg = button;
        evt->type            = type;
        evt->flags           = flags;
        
#if MYNEWT_VAL(BUTTON_USE_PER_BUTTON_CALLBACK_EVENTQ) > 0
        os_eventq_put(button->eventq,              &evt->os_event);
#else
        os_eventq_put(button_callback_default_evq, &evt->os_event);
#endif
        return 0;
    } else {
        button->state |= BUTTON_FLG_MISSED;
        return -1;
    }
}

#if MYNEWT_VAL(BUTTON_USE_EMULATION)
/**
 * Perform button emulation.
 *
 * @param button        the button to emulate
 */
static void
button_emulating(button_t *button)
{
    bool pressed  = true;
    bool released = true;
    
    for (button_t **from = button->emulated ; *from ; from++) {
        if ((*from)->state & BUTTON_FLG_PRESSED) {
            released = false;
        } else {
            pressed  = false;
        }
    }
    
    if (pressed != !released)
        return;

    if (pressed || (released && button->emulating)) {
        button->emulating = pressed;
        button_set_low_level_state(button, pressed);
    }
}

/**
 * Process children of a button. Used for emulating button.
 *
 * @param button        the button to look for children
 */
static void
button_process_children(button_t *button)
{
    if (button->children == NULL)
        return;

    for (button_t **child = button->children ; *child ; child++)
        if ((*child)->emulated)
            button_emulating(*child);
}

/**
 * Check if one of the children is using the action, and so
 * we shouldn't generate one for ourself.
 *
 * @param button        the button to look for children
 */
static bool
button_stealed_action(button_t *button)
{
    if (button->children == NULL)
        return false;

    for (button_t **child = button->children ; *child ; child++)
        if ((*child)->emulated && (*child)->emulating)
            return true;

    return false;
}
#endif

/**
 * Use low level action, to generated button state and action.
 * This one deal with simple state/action, where only pressed/released/click
 * are considered.
 *
 * @param button        button to process
 * @param action        low level action associated to the button
 *                      (_BUTTON_PRESSED, _BUTTON_RELEASED)
 */
static void
button_exec_simple(button_t *button, int action)
{
    assert(action != _BUTTON_TIMEOUT);

    switch(action) {
    case _BUTTON_PRESSED:
        button->state  =  BUTTON_FLG_PRESSED;
        break;
    case _BUTTON_RELEASED:
#if MYNEWT_VAL(BUTTON_EMIT_ACTION)
#if MYNEWT_VAL(BUTTON_USE_EMULATION)
        if (!button_stealed_action(button))
#endif
            BUTTON_POST_ACTION_EVENT(button, button->state);
#endif
        button->state &= ~BUTTON_FLG_PRESSED;
        break;
    }

#if MYNEWT_VAL(BUTTON_EMIT_STATE_CHANGED)
    BUTTON_POST_STATE_EVENT(button, button->state);
#endif
    
#if MYNEWT_VAL(BUTTON_USE_EMULATION)
    button_process_children(button);
#endif
}

#if MYNEWT_VAL(BUTTON_USE_DOUBLE) || \
    MYNEWT_VAL(BUTTON_USE_LONG  ) || \
    MYNEWT_VAL(BUTTON_USE_REPEAT)
/**
 * Use low level action, to generated button state and action.
 * This one deal with complexe action, such as generating double click, 
 * long click, repeating action, ...
 *
 * @param button        the button to process
 * @param action        low level action associated to the button
 *                      (_BUTTON_PRESSED, _BUTTON_RELEASED, _BUTTON_TIMEOUT)
 */
static void
button_exec_fsm(button_t *button, int action)
{
    struct os_callout *callout;

    callout = &button->callout;
    
    switch (button->fsm_state) {
    case _BUTTON_FSM_INIT:
        switch(action) {
        case _BUTTON_PRESSED:
            goto do_pressed;
        case _BUTTON_RELEASED:
            goto do_nothing;
        case _BUTTON_TIMEOUT:
            goto do_assert;
        }
        break;

    case _BUTTON_FSM_PRESSED:
        switch(action) {
        case _BUTTON_PRESSED:
            goto do_nothing;
        case _BUTTON_RELEASED:
#if MYNEWT_VAL(BUTTON_USE_DOUBLE)
            if (button->mode & BUTTON_FLG_DOUBLED) {
                goto to_wait_dblpressed;
            } else {
#endif
                goto do_release;
#if MYNEWT_VAL(BUTTON_USE_DOUBLE)
            }
#endif
        case _BUTTON_TIMEOUT:
#if MYNEWT_VAL(BUTTON_USE_LONG)
            if (button->mode & BUTTON_FLG_LONG) {
                goto do_longpress;
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
            } else if (button->mode & BUTTON_FLG_REPEATING) {
                goto do_repeat;
#endif
            } else {
                goto do_assert;
            }
#elif MYNEWT_VAL(BUTTON_USE_REPEAT)
            goto do_repeat;
#else
            goto do_assert;
#endif
        }
        break;
        
#if MYNEWT_VAL(BUTTON_USE_DOUBLE)
    case _BUTTON_FSM_WAIT_DBLPRESSED:
        switch(action) {
        case _BUTTON_TIMEOUT:
            goto do_release;
        case _BUTTON_PRESSED:
            goto do_dblpressed;
        case _BUTTON_RELEASED:
            goto do_nothing;
        }
        break;

    case _BUTTON_FSM_DBLPRESSED:
        switch(action) {
        case _BUTTON_TIMEOUT :
#if MYNEWT_VAL(BUTTON_USE_LONG)
            if (button->mode & BUTTON_FLG_LONG) {
                goto do_longpress;
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
            } else if (button->mode & BUTTON_FLG_REPEATING) {
                goto do_repeat;
#endif
            } else {
                goto do_assert;
            }
#elif MYNEWT_VAL(BUTTON_USE_REPEAT)
            goto do_repeat;
#else
            goto do_assert;
#endif
        case _BUTTON_RELEASED: goto do_release;
        case _BUTTON_PRESSED : goto do_nothing;
        } 
        break;
#endif

#if MYNEWT_VAL(BUTTON_USE_LONG  ) || \
    MYNEWT_VAL(BUTTON_USE_REPEAT)
    case _BUTTON_FSM_HOLD_OR_REPEAT:
        switch(action) {
        case _BUTTON_TIMEOUT:
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
            if (button->mode & BUTTON_FLG_REPEATING) {
                goto do_repeat;
            } else {
#endif
                goto do_assert;
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
            }
#endif
        case _BUTTON_RELEASED:
            goto do_release;
        case _BUTTON_PRESSED:
            goto do_nothing;
        }
        break;
#endif
    }

 do_assert:
    assert(0);
    
 do_nothing:
    return;
    
 do_pressed:
#if MYNEWT_VAL(BUTTON_USE_LONG)
    if (button->mode & BUTTON_FLG_LONG) {
        os_callout_reset(callout, MYNEWT_VAL(BUTTON_LONGHOLD_TICKS));
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
    } else if (button->mode & BUTTON_FLG_REPEATING) {
        os_callout_reset(callout, MYNEWT_VAL(BUTTON_REPEAT_FIRST_TICKS));
#endif
    }
#elif MYNEWT_VAL(BUTTON_USE_REPEAT)
    os_callout_reset(callout, MYNEWT_VAL(BUTTON_REPEAT_FIRST_TICKS));
#endif
    button->state = BUTTON_FLG_PRESSED;
#if MYNEWT_VAL(BUTTON_EMIT_STATE_CHANGED)
    BUTTON_POST_STATE_EVENT(button, button->state);
#endif
#if MYNEWT_VAL(BUTTON_USE_EMULATION)
    button_process_children(button);
#endif
    button->fsm_state = _BUTTON_FSM_PRESSED;
    return;
    
#if MYNEWT_VAL(BUTTON_USE_DOUBLE)
 to_wait_dblpressed:
    os_callout_reset(callout, MYNEWT_VAL(BUTTON_DBLCLICK_TICKS));
    button->fsm_state = _BUTTON_FSM_WAIT_DBLPRESSED;
    return;
    
 do_dblpressed:
#if MYNEWT_VAL(BUTTON_USE_LONG)
    if (button->mode & BUTTON_FLG_LONG) {
        os_callout_reset(callout, MYNEWT_VAL(BUTTON_LONGHOLD_TICKS));
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
    } else if (button->mode & BUTTON_FLG_REPEATING) {
        os_callout_reset(callout, MYNEWT_VAL(BUTTON_REPEAT_FIRST_TICKS)); 
#endif
    } else {
        os_callout_stop(callout);
    }   
#elif MYNEWT_VAL(BUTTON_USE_REPEAT)
    os_callout_reset(callout, MYNEWT_VAL(BUTTON_REPEAT_FIRST_TICKS)); 
#else
    os_callout_stop(callout);
#endif
    button->state |= BUTTON_FLG_DOUBLED;
#if MYNEWT_VAL(BUTTON_EMIT_STATE_CHANGED)
    BUTTON_POST_STATE_EVENT(button, button->state);
#endif
    button->fsm_state = _BUTTON_FSM_DBLPRESSED;
    return;
#endif

#if MYNEWT_VAL(BUTTON_USE_LONG)
 do_longpress:
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
    if (button->mode & BUTTON_FLG_REPEATING)
        os_callout_reset(callout, MYNEWT_VAL(BUTTON_REPEAT_FIRST_TICKS));
#endif
    button->state |= BUTTON_FLG_LONG;
#if MYNEWT_VAL(BUTTON_EMIT_STATE_CHANGED)
    BUTTON_POST_STATE_EVENT(button, button->state);
#endif
    button->fsm_state = _BUTTON_FSM_HOLD_OR_REPEAT;
    return;
#endif
    
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
 do_repeat:
    os_callout_reset(callout, MYNEWT_VAL(BUTTON_REPEAT_TICKS));
    
#if MYNEWT_VAL(BUTTON_EMIT_ACTION)
#if MYNEWT_VAL(BUTTON_USE_EMULATION)
    if (!button_stealed_action(button))
#endif
        BUTTON_POST_ACTION_EVENT(button, button->state);
#endif
    
    if (!(button->state & BUTTON_FLG_REPEATING)) {
        button->state |= BUTTON_FLG_REPEATING;
#if MYNEWT_VAL(BUTTON_EMIT_STATE_CHANGED)
        BUTTON_POST_STATE_EVENT(button, button->state);
#endif
    }
    return;
#endif
    
 do_release:
    os_callout_stop(callout);

#if MYNEWT_VAL(BUTTON_EMIT_ACTION)
#if MYNEWT_VAL(BUTTON_USE_EMULATION)
    if (!button_stealed_action(button)) {
#endif
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
        if (!(button->state & BUTTON_FLG_REPEATING))
#endif
            BUTTON_POST_ACTION_EVENT(button, button->state);
#if MYNEWT_VAL(BUTTON_USE_EMULATION)
    }
#endif
#endif
    button->state &= ~BUTTON_FLG_PRESSED;
#if MYNEWT_VAL(BUTTON_EMIT_STATE_CHANGED)
    BUTTON_POST_STATE_EVENT(button, button->state);
#endif
#if MYNEWT_VAL(BUTTON_USE_EMULATION)
    button_process_children(button);
#endif
    button->fsm_state = _BUTTON_FSM_INIT;
    return;
}


/**
 * Callout function used for processing timeout.
 * Used for processing complexe action.
 *
 * @param ev            event from callout
 */
static void
button_fsm_callout(struct os_event *ev)
{
    button_t *button;

    button = (button_t *)ev->ev_arg;
    button_exec_fsm(button, _BUTTON_TIMEOUT);
}
#endif

void
button_init(button_t *buttons, unsigned int count, button_callback_t cb)
{
#if MYNEWT_VAL(BUTTON_USE_DOUBLE) ||                            \
    MYNEWT_VAL(BUTTON_USE_LONG  ) ||                            \
    MYNEWT_VAL(BUTTON_USE_REPEAT) ||                            \
    MYNEWT_VAL(BUTTON_USE_PER_BUTTON_CALLBACK_EVENTQ) > 0
    unsigned int i;
#endif
    
    if (button_internal_evq == NULL) {
        button_internal_evq = os_eventq_dflt_get();
    }
    if (button_callback_default_evq == NULL) {
        button_callback_default_evq = os_eventq_dflt_get();
    }
    
    button_callback = cb;

#if MYNEWT_VAL(BUTTON_USE_DOUBLE) || \
    MYNEWT_VAL(BUTTON_USE_LONG  ) || \
    MYNEWT_VAL(BUTTON_USE_REPEAT)
    for (i = 0 ; i < count ; i++) {
        button_t *button = &buttons[i];
        os_callout_init(&button->callout, button_internal_evq,
                        button_fsm_callout, button);
    }
#endif

#if MYNEWT_VAL(BUTTON_USE_PER_BUTTON_CALLBACK_EVENTQ) > 0
    for (i = 0 ; i < count ; i++) {
        button_t *button = &buttons[i];
        if (button->eventq == NULL) {
            button->eventq = button_callback_default_evq;
        }
    }
#endif
}

void
button_set_low_level_state(button_t *button, bool pressed)
{
    int action;

    action = pressed ? _BUTTON_PRESSED : _BUTTON_RELEASED;
#if MYNEWT_VAL(BUTTON_USE_DOUBLE) || \
    MYNEWT_VAL(BUTTON_USE_LONG  ) || \
    MYNEWT_VAL(BUTTON_USE_REPEAT)
    if (button->mode & ~(BUTTON_FLG_PRESSED)) {
        button_exec_fsm(button, action);
    } else {
        button_exec_simple(button, action);
    }
#else
    button_exec_simple(button, action);
#endif
}

void
button_internal_evq_set(struct os_eventq *evq)
{
    button_internal_evq = evq;
}

void
button_callback_default_evq_set(struct os_eventq *evq)
{
    button_callback_default_evq = evq;
}
