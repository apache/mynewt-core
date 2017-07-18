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

#include <os/os.h>
#include <pwm/pwm.h>
#include <pwm_nrf52/pwm_nrf52.h>

static struct os_dev dev;
int arg = 1;

int
main(int argc, char **argv)
{
    struct pwm_dev *pwm;
    os_dev_create(&dev,
                  "pwm",
                  OS_DEV_INIT_KERNEL,
                  OS_DEV_INIT_PRIO_DEFAULT,
                  nrf52_pwm_dev_init,
                  &arg);
    pwm = (struct pwm_dev *) os_dev_open("pwm", 0, NULL);
    pwm_enable_duty_cycle(pwm, 0, 7500);
    return 0;
}
