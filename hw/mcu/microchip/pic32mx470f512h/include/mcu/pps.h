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

/* This file defines the Periphal Pin Select module within this MCU */

#ifndef __MCU_PPS_H__
#define __MCU_PPS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NO_CONNECT      (0)

/* Input */
#define INT3_IN_FUNC    (0)
#define T2CK_IN_FUNC    (1)
#define IC3_IN_FUNC     (2)
#define U1RX_IN_FUNC    (3)
#define U2RX_IN_FUNC    (4)
#define U5CTS_IN_FUNC   (5)
#define REFCLKI_IN_FUNC (6)

#define INT4_IN_FUNC    (16 + 0)
#define T5CK_IN_FUNC    (16 + 1)
#define IC4_IN_FUNC     (16 + 2)
#define U3RX_IN_FUNC    (16 + 3)
#define U4CTS_IN_FUNC   (16 + 4)
#define SDI1_IN_FUNC    (16 + 5)
#define SDI2_IN_FUNC    (16 + 6)

#define INT2_IN_FUNC    (32 + 0)
#define T4CK_IN_FUNC    (32 + 1)
#define IC2_IN_FUNC     (32 + 2)
#define IC5_IN_FUNC     (32 + 3)
#define U1CTS_IN_FUNC   (32 + 4)
#define U2CTS_IN_FUNC   (32 + 5)
#define SS1_IN_FUNC     (32 + 6)

#define INT1_IN_FUNC    (48 + 0)
#define T3CK_IN_FUNC    (48 + 1)
#define IC1_IN_FUNC     (48 + 2)
#define U3CTS_IN_FUNC   (48 + 3)
#define U4RX_IN_FUNC    (48 + 4)
#define U5RX_IN_FUNC    (48 + 5)
#define SS2_IN_FUNC     (48 + 6)
#define OCFA_IN_FUNC    (48 + 7)

/* Output */
#define U3TX_OUT_FUNC       (0b0001)
#define U4RTS_OUT_FUNC      (0b0010)
#define SDO2_OUT_FUNC       (0b0110)
#define OC3_OUT_FUNC        (0b1011)
#define C2OUT_OUT_FUNC      (0b1101)

#define U2TX_OUT_FUNC       (0b0001)
#define U1TX_OUT_FUNC       (0b0011)
#define U5RTS_OUT_FUNC      (0b0100)
#define SDO1_OUT_FUNC       (0b1000)
#define OC4_OUT_FUNC        (0b1011)

#define U3RTS_OUT_FUNC      (0b0001)
#define U4TX_OUT_FUNC       (0b0010)
#define REFCLKO_OUT8_FUNC   (0b0011)
#define U5TX_OUT_FUNC       (0b0100)
#define SS1_OUT_FUNC        (0b0111)
#define OC5_OUT_FUNC        (0b1011)
#define C1OUT_OUT_FUNC      (0b1101)

#define U2RTS_OUT_FUNC      (0b0001)
#define U1RTS_OUT_FUNC      (0b0011)
#define U5TX_OUT_FUNC       (0b0100)
#define SS2_OUT_FUNC        (0b0110)
#define SDO1_OUT_FUNC       (0b1000)
#define OC2_OUT_FUNC        (0b1011)
#define OC1_OUT_FUNC        (0b1100)

/**
 * @brief Configure pin as a peripheral output
 *
 * @param pin
 * @param func
 * @return 0 if successful, -1 otherwise
 */
int pps_configure_output(uint8_t pin, uint8_t func);

/**
 * @brief Configure pin as a peripheral input
 *
 * @param pin
 * @param func
 * @return 0 if successful, -1 otherwise
 */
int pps_configure_input(uint8_t pin, uint8_t func);

#ifdef __cplusplus
}
#endif

#endif /* __MCU_PPS_H__ */
