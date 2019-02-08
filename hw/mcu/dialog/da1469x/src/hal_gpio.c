
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

#include "hal/hal_gpio.h"

int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    return 0;
}

int
hal_gpio_init_out(int pin, int val)
{
    return 0;
}

void
hal_gpio_write(int pin, int val)
{
}

int
hal_gpio_read(int pin)
{
    return 0;
}

int
hal_gpio_toggle(int pin)
{
    int new_value = hal_gpio_read(pin) == 0;

    hal_gpio_write(pin, new_value);

    return new_value;
}
