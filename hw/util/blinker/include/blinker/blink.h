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

#ifndef _BLINK_H_
#define _BLINK_H_

#include <stdbool.h>
#include <os/os.h>

/**
 * The blink, allow led blinking (or beeper).
 * You will need to leverage the gpio or the pwm (with/without easing)
 * to control the line, this is done by providing an implementation
 * of the "on" and "off" methods.
 *
 * Blinking is done by pushing callouts to the event queue, so
 * the 'on' and 'off' methods will be executed in the event queue context.
 * (default queue can be configured with the blink_evq_set())
 *
 *
 * A small example:
 *
 * void led_set_state(bool state) {
 *    hal_gpio_write(LED_2, !state);
 * }
 * hal_gpio_init_out(LED_2, 1);
 *
 * blink_t blink = {
 *   .set_state = led_set_state,
 *   .separator = 2,
 * };
 *
 * blink_init(&blink);
 * blink_code(&blink, BLINK_STREAK(1,1,3), BLINK_SCHEDULED_WAIT_BLINK);
 * blink_dotdash(&blink, "...---...", BLINK_SCHEDULED_WAIT_BLINK);
 */

/**
 * Fast time unit: 0.1s
 */
#define BLINK_UNIT_FAST                 (OS_TICKS_PER_SEC / 10)
/**
 * Medium time unit: 0.25s
 */
#define BLINK_UNIT_MEDIUM               (OS_TICKS_PER_SEC / 4)
/**
 * Slow time unit: 0.5s
 */
#define BLINK_UNIT_SLOW                 (OS_TICKS_PER_SEC / 2)

/**
 * Immediately start the new blinking sequence, whatever
 * is the current blinking state (but still ensure separator time).
 */
#define BLINK_SCHEDULED_IMMEDIATE               1
/**
 * Start the blinking sequence when on an 'off' state.
 */
#define BLINK_SCHEDULED_WAIT_BLINK              2
/**
 * Start the blinking sequence when the current sequence
 * is considered as finished (or whatever is considered
 * acceptable for the current blinking processor)
 */
#define BLINK_SCHEDULED_WAIT_SEQUENCE           3

/**
 * @brief Hold a blink definition.
 *
 * @details Use the BLINK macro to assign a value.
 */
typedef uint32_t blink_code_t;

/**
 * @brief Retrieve the blink length
 *
 * @param  blink                blink value
 * @return Blink length
 */
#define BLINK_GET_LENGTH(blink) ((blink >>  0) & 0xFF)

/**
 * @brief Retrieve the blink delay
 *
 * @param  blink                blink value
 * @return Blink delay
 */
#define BLINK_GET_DELAY(blink)  ((blink >>  8) & 0xFF)

/**
 * @brief Retrieve the blink wait
 *
 * @param  blink                blink value
 * @return Blink wait
 */
#define BLINK_GET_WAIT(blink)   ((blink >> 16) & 0xFF)

/**
 * @brief Retrieve the blink count
 *
 * @param  blink                blink value
 * @return Blink count
 */
#define BLINK_GET_COUNT(blink)  ((blink >> 24) & 0x0F)

/**
 * @brief Define the blink value
 *
 * @note  Special meaning is given to the zero value for
 *        length, delay, wait, or count.
 *        Only the following set containing 0 is well defined:
 *          L / D / C / W
 *          0 / 0 / 0 / 0 = Always off
 *          1 / 0 / 0 / 0 = Always on
 *          x / y / 0 / . = Continous blinking on (length) / off (delay)
 *          x / . / 1 / . = Blink once
 *          x / y / z / 0 = Repeat z time the on/off sequence and stop
 *
 * @details Create the blinking sequence:
 *          (on[length] / (off[delay] / on[length]){count-1} off[wait])+
 *
 * @param length        duration of led on                   (1 .. 255)
 * @param delay         duration of led off (betwenn led on) (1 .. 255)
 * @param count         define a group of consecutive on/off (1 .. 15)
 * @param wait          duration of led off (between group)  (1 .. 255)
 */
#define BLINK(length, delay, count, wait)  ((length <<  0) |    \
                                            (delay  <<  8) |    \
                                            (wait   << 16) |    \
                                            (count  << 24))

/**
 * Always ON
 */
#define BLINK_ON                           BLINK(1, 0, 0, 0)

/**
 * Always OFF
 */
#define BLINK_OFF                          BLINK(0, 0, 0, 0)

/**
 * Blink once and stop
 *
 * @param length                duration of 'on' state
 */
#define BLINK_ONCE(length)                 BLINK(length, 0, 1, 0)

/**
 * Continuously blink
 *
 * @param length                duration of 'on' state
 * @param delay                 duration of 'off' state
 */
#define BLINK_CONTINUOUS(length, delay)    BLINK(length, delay, 0, 0)

/**
 * Blink n-time and stop.
 *
 * @param length                duration of 'on' state
 * @param delay                 duration of 'off' state
 * @param count                 number of blinks
 */
#define BLINK_STREAK(length, delay, count) BLINK(length, delay, count, 0)

/* Forward declaration */
struct blink;

typedef struct blink blink_t;

/*
 * Compute time for next on/off duration
 */
typedef bool (*blink_onoff_t)(blink_t *blink, uint16_t *on, uint16_t *off);

/**
 * Function to set the led/beeper/... state (on / off)
 */
typedef void (*blink_state_t )(bool state);

/**
 * Blink handler
 */
struct blink {
    /**
     * Function to drive the led/beeper/... state (on / off)
     */
    blink_state_t set_state;
    /**
     * Unit of time for blinks (in ticks).
     */
    uint32_t time_unit;
    /**
     * Blink separator. Delay between two consecutive blink requests.
     */
    uint16_t separator;
    /*
     * Last blink time
     */
    os_time_t last_time;
    /*
     * Running 
     */
    struct {
        blink_onoff_t onoff;
        void *data;
        uint16_t step;
        uint8_t flags;
    } running;
    /*
     * Pending 
     */
    struct {
        blink_onoff_t onoff;
        void *data;
        uint8_t scheduled;
    } next;
    /*
     * Mutex
     */
    struct os_mutex mutex;
    /*
     * Callout for 'on'/'off'
     */
    struct os_callout onoff_callout;
    /*
     * State value for callout
     */
    bool c_state;
    int32_t c_next;
};

/**
 * Specify an alternate default queue for processing blink callback.
 *
 * @note If not called, the default OS eventq will be used: os_eventq_dflt_get()
 *
 * @note Calling this function afer blink initialisation has been done
 *       will result in an undefined behaviour.
 */
void blink_evq_set(struct os_eventq *evq);

/**
 * Blink handler initialisation.
 *
 * @param blink                 blink handler
 */
void blink_init(blink_t *blink);

/**
 * Start blinking sequence.
 *
 * @param blink                 blink handler
 * @param code                  Blinkink sequence description
 *                               (see: BLINK macro)
 * @param scheduled             when to start the blinking sequence:
 *                                o BLINK_SCHEDULED_IMMEDIATE
 *                                o BLINK_SCHEDULED_WAIT_BLINK
 *                                o BLINK_SCHEDULED_WAIT_SEQUENCE
 */
void blink_code(blink_t *blink, blink_code_t code, int scheduled);

/**
 * Schedule blinking sequence using short and long blink.
 * Can be used to emit morse code.
 *
 * @param blink                 blink handler
 * @param dotdash               string using '.' and '-'
 * @param scheduled             when to start the blinking sequence:
 *                                o BLINK_SCHEDULED_IMMEDIATE
 *                                o BLINK_SCHEDULED_WAIT_BLINK
 *                                o BLINK_SCHEDULED_WAIT_SEQUENCE
 */
void blink_dotdash(blink_t *blink, char *dotdash, int scheduled);

/**
 * Stop the blinking sequence.
 *
 * @note In this case the 'separator' time won't be applied.
 *
 * @param blink                 blink handler
 * @param scheduled             when to stop the blinking sequence:
 *                                o BLINK_SCHEDULED_IMMEDIATE
 *                                o BLINK_SCHEDULED_WAIT_BLINK
 *                                o BLINK_SCHEDULED_WAIT_SEQUENCE
 */
void blink_stop(blink_t *blink, int scheduled);

#endif
