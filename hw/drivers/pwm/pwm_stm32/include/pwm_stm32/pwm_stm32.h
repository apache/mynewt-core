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

#ifndef __PWM_STM32_H__
#define __PWM_STM32_H__

#include <pwm/pwm.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STM32_PWM_ERR_OK       0
#define STM32_PWM_ERR_NODEV   -1
#define STM32_PWM_ERR_NOTIM   -2
#define STM32_PWM_ERR_CHAN    -3
#define STM32_PWM_ERR_FREQ    -4
#define STM32_PWM_ERR_GPIO    -5

typedef struct stm32_pwm_conf {
    TIM_TypeDef   *tim;
    uint16_t       irq;
} stm32_pwm_conf_t;

/**
 * All HW timers capable of PWM are supported. A maximum of 4 channels per
 * timer can be configured, depending on the HW timer being used.
 */
int stm32_pwm_dev_init(struct os_dev *dev, void *a_struct_stm32_pwm_conf);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_STM32_H__ */
