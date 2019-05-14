/*!
 * \file      utilities.h
 *
 * \brief     Helper functions implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
#ifndef __LORA_UTILITIES_H__
#define __LORA_UTILITIES_H__

#include <stdint.h>

/*!
 * \brief Read the current time
 *
 * \retval time returns current time
 */
uint32_t timer_get_current_time(void);

/*!
 * \brief Return the Time elapsed since a fix moment in Time
 *
 * \param [IN] saved_time   fix moment in Time
 * \retval time             returns elapsed time
 */
uint32_t timer_get_elapsed_time(uint32_t saved_time);

#endif // __LORA_UTILITIES_H__
