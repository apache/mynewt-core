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

#include "temp/temp.h"

int
temp_set_callback(struct temperature_dev *temp_dev, temperature_cb callback, void *arg)
{
    temp_dev->callback = callback;
    temp_dev->callback_arg = arg;
    return 0;
}

int
temp_sample(struct temperature_dev *temp_dev)
{
    return temp_dev->temp_funcs.temp_sample(temp_dev);
}

temperature_t
temp_get_last_sample(struct temperature_dev *temp_dev)
{
    return temp_dev->last_temp;
}

void
temp_sample_completed(struct temperature_dev * temp_dev, temperature_t sample)
{
    temp_dev->last_temp = sample;
    if (temp_dev->callback) {
        temp_dev->callback(temp_dev, temp_dev->callback_arg);
    }
}
