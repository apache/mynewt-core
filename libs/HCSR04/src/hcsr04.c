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
#include <sys/types.h>
#include <assert.h>
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include "hcsr04.h"

void hcsr04_init(void) {
    int rc;
    
    /* set the trigger pin to low */
    rc = hal_gpio_init_out(hcsr04_trigger_pin, 0);
    assert(rc == 0);
    
    /* set the input pin to regular TTL */
    rc = hal_gpio_init_in(hcsr04_echo_pin, GPIO_PULL_NONE);
    assert(rc == 0);
}

/**
 * Measures the distance on the hcsr04.
 * @returns the number of centimeters.
 */
int hcsr04_measure_distance(void) {
    uint32_t start_time, end_time, pulse_time ;
    int ticks, usecs, cm;    
        
    /* don't worry about interrupts and other interruptions as
     * this is 58 usec per centimeter, so even a long interrupt
     * won't add much error */
    
    if(hal_gpio_read(hcsr04_echo_pin) != 0) {
        return -1;
    }
    
    /* send a 10 usec pulse on the trigger */
    hal_gpio_set(hcsr04_trigger_pin);
    cputime_delay_usecs(10);
    hal_gpio_clear(hcsr04_trigger_pin);
    start_time = cputime_get32();    /* falling edge trigger timer */
    
    /* wait for high edge to start timer */
    do {
        pulse_time = cputime_get32();
        ticks = pulse_time - start_time;   
        usecs = cputime_ticks_to_usecs(ticks);
        if(usecs > 2000)
        {
            return -1;
        }
   } while(hal_gpio_read(hcsr04_echo_pin) == 0);

    /* wait for low edge to stop timer */
    do {
        /* wait around for the echo */
        end_time = cputime_get32();
        
        ticks = end_time - pulse_time;   
        usecs = cputime_ticks_to_usecs(ticks);        
        cm = usecs/58;      /* from the data sheet */ 

        /* doubtful this device could measure more than 1 meter.
         * Something went wrong */
        if(cm > 100) {
            return -1;
        }
    }  while(hal_gpio_read(hcsr04_echo_pin) == 1);

    
    return cm;
}

