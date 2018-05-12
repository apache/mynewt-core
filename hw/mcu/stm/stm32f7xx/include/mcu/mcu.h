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

#ifndef __MCU_MCU_H_
#define __MCU_MCU_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Defines for naming GPIOs.
 */
#define MCU_GPIO_PORTA(pin)	((0 * 16) + (pin))
#define MCU_GPIO_PORTB(pin)	((1 * 16) + (pin))
#define MCU_GPIO_PORTC(pin)	((2 * 16) + (pin))
#define MCU_GPIO_PORTD(pin)	((3 * 16) + (pin))
#define MCU_GPIO_PORTE(pin)	((4 * 16) + (pin))
#define MCU_GPIO_PORTF(pin)	((5 * 16) + (pin))
#define MCU_GPIO_PORTG(pin)	((6 * 16) + (pin))
#define MCU_GPIO_PORTH(pin)	((7 * 16) + (pin))
#define MCU_GPIO_PORTI(pin)	((8 * 16) + (pin))
#define MCU_GPIO_PORTJ(pin)	((9 * 16) + (pin))
/* NOTE: PORTK only have pins 0, 1, 3, 4, 5, 6, 7 available */
#define MCU_GPIO_PORTK(pin)	((10 * 16) + (pin))

/*
 * Mapping of a GPIO pin to an alternate function.
 */
#define MCU_AFIO_GPIO(pin,af)  (((0x0F & af) << 12) | (0x00FF & pin))

/*
 * Mapping of alternate function pin descriptors.
 */
#define MCU_AFIO_PORTA(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTA(pin))
#define MCU_AFIO_PORTB(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTB(pin))
#define MCU_AFIO_PORTC(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTC(pin))
#define MCU_AFIO_PORTD(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTD(pin))
#define MCU_AFIO_PORTE(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTE(pin))
#define MCU_AFIO_PORTF(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTF(pin))
#define MCU_AFIO_PORTG(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTG(pin))
#define MCU_AFIO_PORTH(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTH(pin))
#define MCU_AFIO_PORTI(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTI(pin))
#define MCU_AFIO_PORTJ(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTJ(pin))
#define MCU_AFIO_PORTK(pin, af)   MCU_AFIO_GPIO(af, MCU_GPIO_PORTK(pin))

/*
 * Helper macros to extract components from a pin descriptor.
 */
#define MCU_AFIO_PIN_NUM(pin)    (0x000F & pin)
#define MCU_AFIO_PIN_PORT(pin)  ((0x00F0 & pin) >> 4)
#define MCU_AFIO_PIN_PAD(pin)    (0x00FF & pin)
#define MCU_AFIO_PIN_AF(pin)    ((0xF000 & pin) >> 12)

/*
 * Use this value for pins that should not be used.
 */
#define MCU_AFIO_PIN_NONE       (0xFFFF)

#ifdef __cplusplus
}
#endif

#endif /* __MCU_MCU_H_ */
