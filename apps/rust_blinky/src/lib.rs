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

extern "C" {
    fn sysinit_start();
    fn sysinit_app();
    fn sysinit_end();
    fn hal_gpio_init_out(pin: i32, val: i32) -> i32;
    fn hal_gpio_toggle(pin: i32);
    fn os_time_delay(osticks: u32);
}

extern crate panic_halt;

const OS_TICKS_PER_SEC: u32 = 128;

const LED_BLINK_PIN: i32 = 23;


#[no_mangle]
pub extern "C" fn main() {
    /* Initialize all packages. */
    unsafe { sysinit_start(); }
    unsafe { sysinit_app(); }
    unsafe { sysinit_end(); }

    unsafe { hal_gpio_init_out(LED_BLINK_PIN, 1); }

    loop {
        /* Wait one second */
        unsafe { os_time_delay(OS_TICKS_PER_SEC); }

        /* Toggle the LED */
        unsafe { hal_gpio_toggle(LED_BLINK_PIN); }
    }
}
