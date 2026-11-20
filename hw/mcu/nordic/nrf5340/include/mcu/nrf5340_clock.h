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

#ifndef H_NRF5340_CLOCK_
#define H_NRF5340_CLOCK_

#ifdef __cplusplus
 extern "C" {
#endif

/**
 * Request HFXO clock be turned on. Note that each request must have a
 * corresponding release.
 *
 * @return int 0: hfxo was already on. 1: hfxo was turned on.
 */
int nrf5340_clock_hfxo_request(void);

/**
 * Release the HFXO. This means that the caller no longer needs the HFXO to be
 * turned on. Each call to release should have been preceded by a corresponding
 * call to request the HFXO
 *
 *
 * @return int 0: HFXO not stopped by this call (others using it) 1: HFXO
 *         stopped.
 */
int nrf5340_clock_hfxo_release(void);

/**
 * Request low frequency clock source change.
 *
 * @param  clksrc Value to determine requested clock source
 *                This parameter can be one of the following values:
 *                @arg CLOCK_LFCLKSTAT_SRC_LFRC - 32.768 kHz RC oscillator
 *                @arg CLOCK_LFCLKSTAT_SRC_LFXO - 32.768 kHz crystal oscillator
 *                @arg CLOCK_LFCLKSTAT_SRC_LFSYNT - 32.768 kHz synthesized from HFCLK
 *
 * @return int 0: clock source was already as requested. 1: clock source
 *         changed.
 */
int nrf5340_set_lf_clock_source(uint32_t clksrc);

/**
 * Request HFCLK192M clock be turned on. Note that each request must have a
 * corresponding release.
 *
 * @return int 0: HFCLK192M was already on. 1: HFCLK192M was turned on.
 */
int nrf5340_clock_hfclk192m_request(void);

/**
 * Release the HFCLK192M. This means that the caller no longer needs the HFCLK192M to be
 * turned on. Each call to release should have been preceded by a corresponding
 * call to request the HFCLK192M
 *
 *
 * @return int 0: HFCLK192M not stopped by this call (others using it) 1: HFCLK192M
 *         stopped.
 */
int nrf5340_clock_hfclk192m_release(void);

#define usb_clock_request() nrf5340_clock_hfclk192m_request()
#define usb_clock_release() nrf5340_clock_hfclk192m_release()

#ifdef __cplusplus
}
#endif

#endif  /* H_NRF5340_CLOCK_ */
