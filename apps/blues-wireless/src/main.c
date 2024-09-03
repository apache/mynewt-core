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
#include <string.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_i2c.h"
#include "hal/hal_uart.h"
#include <console/console.h>

#include <note.h>
#include "note_c_hooks.h"

/*Product UUID from notehub*/

#define PRODUCT_UID "UID"

float lat, lon;

/* For LED toggling */
int g_led_pin;

static void
init_notecard(void)
{
    NoteSetFnDefault(malloc, free, platform_delay, platform_millis);
    NoteSetFnDebugOutput(note_log_print);
    NoteSetFnI2C(NOTE_I2C_ADDR_DEFAULT, NOTE_I2C_MAX_DEFAULT, note_i2c_reset, note_i2c_transmit, note_i2c_receive);

    J *req = NoteNewRequest("hub.set");
    JAddStringToObject(req, "product", PRODUCT_UID);
    JAddStringToObject(req, "mode", "periodic");
    JAddBoolToObject(req, "sync", true);

    NoteRequestWithRetry(req, 5);

    J *req2 = NoteNewRequest("card.version");

    NoteRequestWithRetry(req2, 5);

    J *req3 = NoteNewRequest("card.location.mode");
    JAddStringToObject(req3, "mode","continuous");

    NoteRequestWithRetry(req3, 5);
}

static void
update_location(void)
{
    J *location = NoteRequestResponse(NoteNewRequest("card.location"));

    if (location != NULL) {
        lat = JGetNumber(location, "lat");
        lon = JGetNumber(location, "lon");
    }
    J *req = NoteNewRequest("note.add");
    JAddStringToObject(req, "file", "location.qo");
    JAddBoolToObject(req, "sync", true);

    J *body = JCreateObject();
    JAddStringToObject(body, "message", "location");

    if (location == NULL) {
        J *timereq = NoteRequestResponse(NoteNewRequest("card.time"));
        if (timereq != NULL) {
            lat = JGetNumber(timereq, "lat");
            lon = JGetNumber(timereq, "lat");
        }
    }
    JAddNumberToObject(body, "Latitude", lat);
    JAddNumberToObject(body, "Longitude", lon);
    JAddItemToObject(req, "body", body);
    NoteRequest(req);
}

/**
 * main
 *
 * The main task for the project. This function initializes packages,
 * and then blinks the BSP LED in a loop.
 *
 * @return int NOTE: this function should never return!
 */
int
mynewt_main(int argc, char **argv)
{
    int rc;
    sysinit();

    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);
    init_notecard();

    while (1) {
        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC * 10);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);
        update_location();
    }
    assert(0);
    return rc;
}
