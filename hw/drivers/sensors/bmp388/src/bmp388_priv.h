/**\mainpage
 * Copyright (C) 2017 - 2019 Bosch Sensortec GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of the
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 * File   bmp388_priv.h
 * Date   10 May 2019
 * Version   1.0.2
 *
 */

#ifndef __BMP388_PRIV_H__
#define __BMP388_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

#define BMP388_SPI_READ_CMD_BIT            0x80
#define BMP388_REG_WHO_AM_I                0x00


int8_t bmp388_i2c_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);
int8_t bmp388_i2c_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint16_t len);

int8_t bmp388_spi_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint16_t len);

int8_t bmp388_readlen(struct bmp388 *bmp388, uint8_t reg, uint8_t *buffer, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BMP388_PRIV_H_ */
