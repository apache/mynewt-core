/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   mcu.h
 * Author: paulfdietrich
 *
 * Created on March 25, 2016, 3:07 PM
 */

#ifndef MCU_DEVID_H
#define MCU_DEVID_H

#ifdef __cplusplus
extern "C" {
#endif

/* this is a native build and these pins are not real.  They are used only
 * for simulated HAL devices */
enum McuDeviceDescriptor {
        /* supports random ADC */
    MCU_ADC_CHANNEL_0   = 0,
    MCU_ADC_CHANNEL_1   = 1,
        /* supports min mid max ADC */
    MCU_ADC_CHANNEL_2   = 2,
    MCU_ADC_CHANNEL_3   = 3,
    MCU_ADC_CHANNEL_4   = 4,
        /* support file Input ADC */
    MCU_ADC_CHANNEL_5   = 5,
};

#ifdef __cplusplus
}
#endif

#endif /* MCU_DEVID_H */
