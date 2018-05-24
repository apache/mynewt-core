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

#ifndef _BUTTON_H_
#define _BUTTON_H_

#include <stdbool.h>
#include "os/mynewt.h"

/**
 * This library generate events in the default queue to deal 
 * with user interactions. These events need to be processed
 * (by the main program or a thread) so that the associated 
 * callback is triggered.
 *
 * According to the selected mode it is possible to deal with actions
 * such as: click, double click, long click, long double click.
 * And to make the last action auto-repeat (like a key).
 *
 * It can also generate event to reflect the actual button state,
 * which is a mix of: pressed / doubled / long / repeating.
 *
 * The action differ from the state, as the action is generated
 * when the button is released or after a small timeout.
 *
 * When a button is actively participating in an emulation, the state
 * change event is emited but the action is not.
 * For example button 3 = button 1 + button 2
 *    a simultaneous click on button 1 and 2, will emit 
 *    a click for button 3, but neither for 1 or 2.
 *
 *
 *
 * Small example, using the debounce package to drive the buttons.
 *
 * 
 *
 * button_t btns[] = {
 *    { .id       = 1,
 *      .children = (button_t *[]){ &btns[2], NULL },
 *      .mode     = BUTTON_MODE_MOUSE ,
 *    },
 *    { .id       = 2,
 *      .children = (button_t *[]){ &btns[2], NULL },
 *      .mode     = BUTTON_MODE_TOUCH | BUTTON_FLG_REPEATING, 
 *    },
 *    { .id       = 3,
 *      .emulated = (button_t *[]){ &btns[0], &btns[1], NULL },
 *      .mode     = BUTTON_MODE_BUTTON, 
 *    },
 * };
 *
 * button_init(btns, 3, button_callback);
 *
 *
 * void button_debounce_handler(debounce_pin_t *d) {
 *    button_t *button = (button_t *) d->arg;
 *    bool   pressed = !debounce_state(d);
 *
 *    button_set_low_level_state(button, pressed);
 * }
 *
 * debounce_pin_t dbtn_1, dbtn_2;
 * debounce_init(&dbtn_1, BUTTON_1, HAL_GPIO_PULL_UP, 5);
 * debounce_init(&dbtn_2, BUTTON_2, HAL_GPIO_PULL_UP, 5);
 *
 * debounce_start(&dbtn_1, DEBOUNCE_CALLBACK_EVENT_ANY,
 *                button_debounce_handler, &btns[0]);
 * debounce_start(&dbtn_2, DEBOUNCE_CALLBACK_EVENT_ANY,
 *                button_debounce_handler, &btns[1]);
 *
 *
 *
 * void button_callback(button_id_t id, uint8_t type, uint8_t flags) {
 *   if (type != BUTTON_ACTION) // Only interested by action
 *      return;
 *
 *   switch(id) {
 *   case 1:
 *     if (flags & BUTTON_FLG_MISSED)
 *       alert("some action was lost");
 *     console_printf("BUTTON[%d] ", id);
 *     if (flags & BUTTON_FLG_PRESSED) {
 *       console_printf("ACTION=");
 *       if (flags & BUTTON_FLG_LONG)      console_printf("long ");
 *       if (flags & BUTTON_FLG_DOUBLED)   console_printf("double ");
 *       console_printf("click");
 *       if (flags & BUTTON_FLG_REPEATING) console_printf(" repeated");
 *       console_printf("\n");
 *     }
 *     break;
 *   case 2:
 *     // Look at action, 
 *     // but don't make special handling for repeated or missed
 *     switch(flags & BUTTON_MASK_BASIC) {
 *     case BUTTON_LONG_CLICK:  volume_off();    break;
 *     case BUTTON_CLICK:       volume_dec(1);   break;
 *     case BUTTON_DBLCLICK:    volume_dec(10);  break;
 *     }
 *     break;
 *   }
 * }
 *
 */

/**
 * Allow to only keep the pressed / double / long information.
 * As it is generally usefull to discard the error parts of the flags
 * (missed)
 */
#define BUTTON_MASK_BASIC     0x07

/**
 * Allow to only keep the pressed / double / long / repeat information.
 * A it is generally usefull to discard the error parts of the flags
 * (missed)
 */
#define BUTTON_MASK_FULL      0x0F


/**
 * Pressed state/action
 */
#define BUTTON_FLG_PRESSED              0x01
#if MYNEWT_VAL(BUTTON_USE_DOUBLE)
/**
 * Doubled pressed state/action
 */
#define BUTTON_FLG_DOUBLED              0x02
#endif
#if MYNEWT_VAL(BUTTON_USE_LONG)
/**
 * Long pressed state/action
 */
#define BUTTON_FLG_LONG                 0x04
#endif
#if MYNEWT_VAL(BUTTON_USE_REPEAT)
/**
 * Repeating state, continuously repeating last action
 */
#define BUTTON_FLG_REPEATING            0x08
#endif
/**
 * Some event have been missed (not enough buffer event, 
 *  increase BUTTON_EVENT_MAX)
 */
#define BUTTON_FLG_MISSED               0x40


/**
 * Indicate State Changed
 */
#define BUTTON_STATE_CHANGED            0x01
/**
 * Indicate Action
 */
#define BUTTON_ACTION                   0x02


/**
 * Behave as a standard button (click action)
 */
#define BUTTON_MODE_BUTTON     (BUTTON_FLG_PRESSED)

/**
 * A click action
 */
#define BUTTON_CLICK           (BUTTON_FLG_PRESSED)

/**
 * A pressed state
 */
#define BUTTON_PRESSED         (BUTTON_FLG_PRESSED)

/**
 * A released state
 */
#define BUTTON_RELEASED        (0)


#if MYNEWT_VAL(BUTTON_USE_DOUBLE)
/**
 * Behave as a mouse button (click, double click action)
 */
#define BUTTON_MODE_MOUSE      (BUTTON_MODE_BUTTON | BUTTON_FLG_DOUBLED)

/**
 * A double click action
 */ 
#define BUTTON_DBLCLICK        (BUTTON_CLICK       | BUTTON_FLG_DOUBLED)

/**
 * A double pressed state
 */ 
#define BUTTON_DBLPRESSED      (BUTTON_PRESSED     | BUTTON_FLG_DOUBLED)
#endif

#if MYNEWT_VAL(BUTTON_USE_LONG)
/**
 * Behave as a "pen" (click, and long press)
 */
#define BUTTON_MODE_PEN        (BUTTON_MODE_BUTTON | BUTTON_FLG_LONG)

/**
 * A long click action
 */
#define BUTTON_LONG_CLICK      (BUTTON_CLICK       | BUTTON_FLG_LONG)

/**
 * A long pressed state
 */
#define BUTTON_LONG_PRESSED    (BUTTON_PRESSED     | BUTTON_FLG_LONG)

#endif

#if MYNEWT_VAL(BUTTON_USE_DOUBLE) && MYNEWT_VAL(BUTTON_USE_LONG)
/**
 * Behave as a "touch" (click, double click, long press, double long press)
 */ 
#define BUTTON_MODE_TOUCH      (BUTTON_MODE_MOUSE  | BUTTON_FLG_LONG)
/**
 * A long double click action
 */
#define BUTTON_LONG_DBLCLICK   (BUTTON_DBLCLICK    | BUTTON_FLG_LONG)

/**
 * A long double pressed state
 */
#define BUTTON_LONG_DBLPRESSED (BUTTON_DBLPRESSED  | BUTTON_FLG_LONG)
#endif


#if MYNEWT_VAL(BUTTON_USE_REPEAT)
/**
 * A repeating pressed state
 */
#define BUTTON_PRESSED_REPEATING (BUTTON_PRESSED | BUTTON_FLG_REPEATING)

/**
 * A repeated click action
 */
#define BUTTON_CLICK_REPEATED    (BUTTON_CLICK | BUTTON_FLG_REPEATING)

#if MYNEWT_VAL(BUTTON_USE_LONG)
/**
 * A repeating long pressed state
 */
#define BUTTON_LONG_PRESSED_REPEATING (BUTTON_PRESSED | BUTTON_FLG_REPEATING)

/**
 * A repeated long click action
 */
#define BUTTON_LONG_CLICK_REPEATED    (BUTTON_CLICK | BUTTON_FLG_REPEATING)
#endif

#if MYNEWT_VAL(BUTTON_USE_DOUBLE)
/**
 * A repeating double pressed state
 */
#define BUTTON_DBLPRESSED_REPEATING (BUTTON_DBLPRESSED | BUTTON_FLG_REPEATING)

/**
 * A repeated double click action
 */
#define BUTTON_DBLCLICK_REPEATED    (BUTTON_DBLCLICK | BUTTON_FLG_REPEATING)
#endif

#if MYNEWT_VAL(BUTTON_USE_DOUBLE) && MYNEWT_VAL(BUTTON_USE_LONG)
/**
 * A repeating long double pressed state
 */
#define BUTTON_LONG_DBLPRESSED_REPEATING (BUTTON_LONG_DBLPRESSED | BUTTON_FLG_REPEATING)

/**
 * A repeated long double click action
 */
#define BUTTON_LONG_DBLCLICK_REPEATED    (BUTTON_LONG_DBLCLICK | BUTTON_FLG_REPEATING)
#endif
#endif

/**
 * Button id
 */
typedef uint8_t button_id_t;

/**
 * Button definition
 */
typedef struct button {
    /**
     * Button identifier.
     */
    button_id_t id;
    /**
     * Button behaviour.
     * Type of state changed or action that should be taken into account.
     */
    uint8_t mode;
#if MYNEWT_VAL(BUTTON_USE_PER_BUTTON_CALLBACK_EVENTQ) > 0
    /**
     * Specific Event queue for this button callback.
     * If not defined (ie: is NULL), the default event queue
     * specified by button_callback_default_evq_set() will be used.
     */
    struct os_eventq *eventq;
#endif
    /*
     * Current button state
     */
    uint8_t state;
#if MYNEWT_VAL(BUTTON_USE_DOUBLE) || \
    MYNEWT_VAL(BUTTON_USE_LONG  ) || \
    MYNEWT_VAL(BUTTON_USE_REPEAT)
    /*
     * Finite state machine state
     */
    uint8_t fsm_state;
    /*
     * Callout for driving FSM timeout
     */
    struct os_callout callout;
#endif
#if MYNEWT_VAL(BUTTON_USE_EMULATION)
    /**
     * List (NULL-terminated) of buttons participating in the emulation.
     * You will need to populate the from field for this button
     * and children fields of the participating buttons.
     */
    struct button **emulated;
    /**
     * List (NULL-terminated) of buttons depending on this one.
     */
    struct button **children; 
    /*
     * Is this button currently behing emulated
     */
    bool emulating;      
#endif
#if MYNEWT_VAL(BUTTON_USE_FILTERING)
    struct {
        /**
         * Enable filtering (reducing number of events emited)
         */
        bool enabled;
        /**
         * List of state change to emit, based on BUTTON_FLG_*
         */
        uint8_t state;
        /**
         * List of action to emit, base on BUTTON_FLG_*
         */
        uint8_t action;
    } filter;
#endif
} button_t;

/**
 * Definition of the callback use to notify of action or state change.
 *
 * @param button        concerned button
 * @param type          type of event (BUTTON_STATE_CHANGED or BUTTON_ACTION)
 * @param flags         flag indicating the action or state change
 *                      (see BUTTON_FLG_*)
 */
typedef void (*button_callback_t)(button_id_t id, uint8_t type, uint8_t flags);

/**
 * Drive the button, by setting the the low level state (pressed / released)
 * when it happens.
 *
 * @param button        button
 * @param pressed       low level button state
 */
void button_set_low_level_state(button_t *button, bool pressed);

/**
 * Specify an alternate queue for the buttons internal management.
 *
 * @note If not called, the default OS eventq will be used: os_eventq_dflt_get()
 *
 * @note Calling this function afer button initialisation has been done
 *       will result in an undefined behaviour.
 */
void button_internal_evq_set(struct os_eventq *evq);

/**
 * Specify an alternate default queue for processing button callback.
 *
 * @note If not called, the default OS eventq will be used: os_eventq_dflt_get()
 *
 * @note Calling this function afer button initialisation has been done
 *       will result in an undefined behaviour.
 */
void button_callback_default_evq_set(struct os_eventq *evq);

/**
 * Initialisation of the buttons
 *
 * @param buttons       buttons definition
 * @param count         number of defined buttons
 * @param cb            callback used to notify of state change or action
 */
void button_init(button_t *buttons, unsigned int count, button_callback_t cb);

#endif
