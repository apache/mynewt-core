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

#ifndef H_HAL_GPIO_
#define H_HAL_GPIO_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The "mode" of the gpio. The gpio is either an input, output, or it is
 * "not connected" (the pin specified is not functioning as a gpio)
 */
enum hal_gpio_mode_e {
    HAL_GPIO_MODE_NC = -1,
    HAL_GPIO_MODE_IN = 0,
    HAL_GPIO_MODE_OUT = 1
};
typedef enum hal_gpio_mode_e hal_gpio_mode_t;

/*
 * The "pull" of the gpio. This is either an input or an output.
 */
enum hal_gpio_pull {
    HAL_GPIO_PULL_NONE = 0,     /* pull-up/down not enabled */
    HAL_GPIO_PULL_UP = 1,       /* pull-up enabled */
    HAL_GPIO_PULL_DOWN = 2      /* pull-down enabled */
};
typedef enum hal_gpio_pull hal_gpio_pull_t;

/*
 * IRQ trigger type.
 */
enum hal_gpio_irq_trigger {
    HAL_GPIO_TRIG_NONE = 0,
    HAL_GPIO_TRIG_RISING = 1,   /* IRQ occurs on rising edge */
    HAL_GPIO_TRIG_FALLING = 2,  /* IRQ occurs on falling edge */
    HAL_GPIO_TRIG_BOTH = 3,     /* IRQ occurs on either edge */
    HAL_GPIO_TRIG_LOW = 4,      /* IRQ occurs when line is low */
    HAL_GPIO_TRIG_HIGH = 5      /* IRQ occurs when line is high */
};
typedef enum hal_gpio_irq_trigger hal_gpio_irq_trig_t;

/* Function proto for GPIO irq handler functions */
typedef void (*hal_gpio_irq_handler_t)(void *arg);

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
int hal_gpio_init_in(int pin, hal_gpio_pull_t pull);

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
int hal_gpio_init_out(int pin, int val);

/**
 * gpio write
 *
 * Write a value (either high or low) to the specified pin.
 *
 * @param pin Pin to set
 * @param val Value to set pin (0:low 1:high)
 */
void hal_gpio_write(int pin, int val);

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
int hal_gpio_read(int pin);

/**
 * gpio toggle
 *
 * Toggles the specified pin
 *
 * @param pin Pin number to toggle
 *
 * @return current gpio state int 0: low, 1: high
 */
int hal_gpio_toggle(int pin);

int hal_gpio_irq_init(int pin, hal_gpio_irq_handler_t handler, void *arg,
                      hal_gpio_irq_trig_t trig, hal_gpio_pull_t pull);
void hal_gpio_irq_release(int pin);
void hal_gpio_irq_enable(int pin);
void hal_gpio_irq_disable(int pin);


#ifdef __cplusplus
}
#endif

#endif /* H_HAL_GPIO_ */
