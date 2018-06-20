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

/*
 * All HW timers capable of PWM are supported. A maximum of 4 channels per
 * timer can be configured, depending on the HW timer being used.
 *
 * Currently there is no support for complementary outputs.
 *
 * MCU_AFIO_PIN_NONE can be used in pwm_configure_channel in order
 * 'unconfigure' a previously configured PWM IO pin.
 *
 * The driver can return one of several error codes in order to aid issue
 * tracking.
 *
 * STM32_PWM_ERR_OK      ... no error
 * STM32_PWM_ERR_NODEV   ... no devices available, depending on configuration
 *                           up to 3 devices are supported
 * STM32_PWM_ERR_NOTIM   ... no hw timer was specified for initialization
 * STM32_PWM_ERR_CHAN    ... specified channel is not valid for device
 * STM32_PWM_ERR_FREQ    ... either no frequency was specified, or the specified
 *                           frequency is higher than the clock frequency
 * STM32_PWM_ERR_GPIO    ... an error occured during IO pin configuration
 * STM32_PWM_ERR_NOIRQ   ... the device was registered without IRQ support but
 *                           later cycle and/or sequence support was requested
 */

#define STM32_PWM_ERR_OK       0
#define STM32_PWM_ERR_NODEV    1
#define STM32_PWM_ERR_NOTIM    2
#define STM32_PWM_ERR_CHAN     3
#define STM32_PWM_ERR_FREQ     4
#define STM32_PWM_ERR_GPIO     5
#define STM32_PWM_ERR_NOIRQ    6

typedef struct stm32_pwm_conf {
    TIM_TypeDef   *tim;
    uint16_t       irq;
} stm32_pwm_conf_t;

int stm32_pwm_dev_init(struct os_dev *dev, void *struct_stm32_pwm_conf_pointer);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_STM32_H__ */
