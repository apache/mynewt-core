/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hal/hal_gpio.h"
#include <assert.h>

 /* XXX: Notes
 * 4) The code probably does not handle "re-purposing" gpio very well. 
 * "Re-purposing" means changing a gpio from input to output, or calling 
 * gpio_init_in and expecting previously enabled interrupts to be stopped. 
 *  
 */

/* 
 * GPIO pin mapping
 *
 * The samD 21G has 48 pins and 38 GPIO.
 * 
 * They are split among 2 ports A (PA0-PA25) and B
 * 
 * , PB8, PB9, 
 *  PB10, PB11, 
 * PB3 PB2,  PB23, PB22,  
 * 
 * The port A Pins exposed on the CHIP are 
 *  PA0,  PA1,  PA2,  PA3,  PA4,  PA5,  PA6,  PA7,
 *  PA8,  PA9, PA10, PA11, PA12, PA13, PA14, PA15, 
 * PA16, PA17, PA18, PA19, PA20, PA21, PA22, PA23, 
 * PA24, PA25,       PA27, PA28,       PA30, PA31, 
 * 
 * The port B pins exposed the chip are
 *              PB2,  PB3, 
 *  PB8,  PB9, PB10,  B11,
 * 
 *                                     PA22, PA23, 
 *
 * 
 * We keep the mapping where ports A0-A31 are pins 0 - 31
 * and ports B0-B31 are pins 32 - 64;
 */ 

#define GPIO_PORT(pin)           (pin/32)
#define GPIO_PIN(pin)            (pin & 32)

#define GPIO_MAX_PORT   (1)

/* this defines which pins are valid for the ports */
int valid_pins[GPIO_MAX_PORT + 1] =
{
    0xdbffffff,
    0xc0000f0c,
};


int gpio_init_out(int pin, int val)
{
    int port = GPIO_PORT(pin);
    int port_pin = GPIO_PIN(pin);
    
    if(port > GPIO_MAX_PORT) {
        return -1;
    }
    
    if((port_pin & valid_pins[port]) == 0) {
        return -1;
    }
 
    return 0;
}

/**
 * gpio set 
 *  
 * Sets specified pin to 1 (high) 
 * 
 * @param pin 
 */
void gpio_set(int pin)
{
    int port = GPIO_PORT(pin);
    int port_pin = GPIO_PIN(pin);
    
    assert(port <= GPIO_MAX_PORT);
    assert((port_pin & valid_pins[port]) == 0);
        
    
}

/**
 * gpio clear
 *  
 * Sets specified pin to 0 (low). 
 * 
 * @param pin 
 */
void gpio_clear(int pin)
{
    int port = GPIO_PORT(pin);
    int port_pin = GPIO_PIN(pin);
    
    assert(port <= GPIO_MAX_PORT);
    assert((port_pin & valid_pins[port]) == 0);    
}

/**
 * gpio read 
 *  
 * Reads the specified pin. 
 *  
 * @param pin Pin number to read
 * 
 * @return int 0: low, 1: high
 */
int gpio_read(int pin)
{
    int port = GPIO_PORT(pin);
    int port_pin = GPIO_PIN(pin);
    
    assert(port <= GPIO_MAX_PORT);
    assert((port_pin & valid_pins[port]) == 0);    
    return 0;
}

/**
 * gpio write 
 *  
 * Write a value (either high or low) to the specified pin.
 * 
 * @param pin Pin to set
 * @param val Value to set pin (0:low 1:high)
 */
void gpio_write(int pin, int val)
{
    if (val) {
        gpio_set(pin);
    } else {
        gpio_clear(pin);
    }
}

/**
 * gpio toggle 
 *  
 * Toggles the specified pin
 * 
 * @param pin Pin number to toggle
 */
void gpio_toggle(int pin)
{
    if (gpio_read(pin)) {
        gpio_clear(pin);
    } else {
        gpio_set(pin);
    }
}