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

#ifndef HCSR04_H
#define HCSR04_H

/* you need to define these in your BSP */
extern const int hcsr04_trigger_pin;
extern const int hcsr04_echo_pin;

/* The HCSR04 is a simple untrasonic distance sensor.  
 * It measures distance via untra-sonic ranging.  The datasheet
 * can be found here http://www.micropik.com/PDF/HCSR04.pdf.  NOTE
 * the actual range is the width of the echo pulse, not the time
 * between the echo pulse and trigger pulse.  */

/* This library implements a simple polling driver for this 
 * part using GPIO. Ideally, this would use two PWM pins, one
 * would drive the part at periodic intervals (say 20 msec).
 * The other would could the width of the echo pulse.  Then
 * a simple API call would just convert this width into 
 * the distance value.  We don't yet have a HAL to do this 
 * stuff, so instead I decided to use the HAL to count this. */

void hcsr04_init(void);

/**
 * Measures the distance on the hcsr04.
 * @returns the number of centimeters.
 */
int hcsr04_measure_distance(void);

#endif /* HCSR04_H */

