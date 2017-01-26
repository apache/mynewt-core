/*****************************************************************************/
/*!
    @file     tsl2561_priv.h
    @author   ktown (Adafruit Industries)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2016, Adafruit Industries (adafruit.com)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*****************************************************************************/
#ifndef __ADAFRUIT_TSL2561_PRIV_H__
#define __ADAFRUIT_TSL2561_PRIV_H__

#define TSL2561_REGISTER_CONTROL          (0x00)
#define TSL2561_REGISTER_TIMING           (0x01)
#define TSL2561_REGISTER_THRESHHOLDL_LOW  (0x02)
#define TSL2561_REGISTER_THRESHHOLDL_HIGH (0x03)
#define TSL2561_REGISTER_THRESHHOLDH_LOW  (0x04)
#define TSL2561_REGISTER_THRESHHOLDH_HIGH (0x05)
#define TSL2561_REGISTER_INTERRUPT        (0x06)
#define TSL2561_REGISTER_ID               (0x0A)
#define TSL2561_REGISTER_CHAN0_LOW        (0x0C)
#define TSL2561_REGISTER_CHAN0_HIGH       (0x0D)
#define TSL2561_REGISTER_CHAN1_LOW        (0x0E)
#define TSL2561_REGISTER_CHAN1_HIGH       (0x0F)

#define TSL2561_CONTROL_POWERON           (0x03)
#define TSL2561_CONTROL_POWEROFF          (0x00)

#define TSL2561_INTEGRATIONTIME_13MS      (0x00)  /* 13.7ms */
#define TSL2561_INTEGRATIONTIME_101MS     (0x01)  /* 101ms */
#define TSL2561_INTEGRATIONTIME_402MS     (0x02)  /* 402ms */

#define TSL2561_GAIN_1X                   (0x00)  /* No gain */
#define TSL2561_GAIN_16X                  (0x10)  /* 16x gain */

#define TSL2561_COMMAND_BIT               (0x80)  /* Must be 1 */
#define TSL2561_CLEAR_BIT                 (0x40)  /* 1=Clear any pending int */
#define TSL2561_WORD_BIT                  (0x20)  /* 1=Read/write word */
#define TSL2561_BLOCK_BIT                 (0x10)  /* 1=Use block read/write */

#define TSL2561_CONTROL_POWERON           (0x03)
#define TSL2561_CONTROL_POWEROFF          (0x00)

#ifdef __cplusplus
extern "C" {
#endif

/* tsl2561.c */
int tsl2561_write8(uint8_t reg, uint32_t value);
int tsl2561_write16(uint8_t reg, uint16_t value);
int tsl2561_read8(uint8_t reg, uint8_t *value);
int tsl2561_read16(uint8_t reg, uint16_t *value);

/* tsl2561_shell.c */
#if MYNEWT_VAL(TSL2561_CLI)
int tsl2561_shell_init(void);
#endif


#ifdef __cplusplus
}
#endif

#endif /* __ADAFRUIT_TSL2561_PRIV_H_ */
