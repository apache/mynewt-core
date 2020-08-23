/*
 * Copyright 2020 Casper Meijn <casper@meijn.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#![no_std]

mod bindings;

extern crate panic_halt;

// Define a safe wrapper for os_time_delay
fn os_time_delay_ms(ms: u32) {
    let mut ticks: bindings::os_time_t = 0;
    let result = unsafe { bindings::os_time_ms_to_ticks(ms, &mut ticks) };
    assert!(result == 0);
    unsafe { bindings::os_time_delay(ticks) };
}

#[no_mangle]
pub extern "C" fn main() {
    /* Initialize all packages. */
    unsafe {
        bindings::sysinit_start();
        bindings::sysinit_app();
        bindings::sysinit_end();
    }

    /* Turn on the LED */
    unsafe {
        bindings::hal_gpio_init_out(bindings::LED_BLINK_PIN as i32, 1);
    }

    loop {
        /* Wait one second */
        os_time_delay_ms(1000);

        /* Toggle the LED */
        unsafe {
            bindings::hal_gpio_toggle(bindings::LED_BLINK_PIN as i32);
        }
    }
}
