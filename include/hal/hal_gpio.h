/**
 * Copyright (c) 2015 Stack Inc.
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

#ifndef H_HAL_GPIO_
#define H_HAL_GPIO_

/*
 * The "mode" of the gpio. The gpio is either an input, output, or it is
 * "not connected" (the pin specified is not functioning as a gpio)
 */
enum gpio_mode_e
{
    GPIO_MODE_NC = -1,
    GPIO_MODE_IN = 0,
    GPIO_MODE_OUT = 1
};

typedef enum gpio_mode_e gpio_mode_t;

/*
 * The "pull" of the gpio. This is either an input or an output.
 */
enum gpio_pull
{
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP = 1,
    GPIO_PULL_DOWN = 2
};

typedef enum gpio_pull gpio_pull_t;

/**
 * gpio init in 
 *  
 * Initializes the specified pin as an input 
 * 
 * @param pin   Pin number to set as input
 * @param pull  pull type
 * 
 * @return int  0: no error; -1 otherwise. 
 */
int gpio_init_in(int pin, gpio_pull_t pull);

/**
 * gpio init out 
 *  
 * Initialize the specified pin as an output, setting the pin to the specified 
 * value. 
 * 
 * @param pin Pin number to set as output
 * @param val Value to set pin
 * 
 * @return int  0: no error; -1 otherwise. 
 */
int gpio_init_out(int pin, int val);

/**
 * gpio set 
 *  
 * Sets specified pin to 1 (high) 
 * 
 * @param pin 
 */
void gpio_set(int pin);

/**
 * gpio clear
 *  
 * Sets specified pin to 0 (low). 
 * 
 * @param pin 
 */
void gpio_clear(int pin);

/**
 * gpio write 
 *  
 * Write a value (either high or low) to the specified pin.
 * 
 * @param pin Pin to set
 * @param val Value to set pin (0:low 1:high)
 */
void gpio_write(int pin, int val);

/**
 * gpio read 
 *  
 * Reads the specified pin. 
 *  
 *  
 * @param pin Pin number to read
 * 
 * @return int 0: low, 1: high
 */
int gpio_read(int pin);

/**
 * gpio toggle 
 *  
 * Toggles the specified pin
 * 
 * @param pin Pin number to toggle
 */
void gpio_toggle(int pin);

#endif /* H_HAL_GPIO_ */
