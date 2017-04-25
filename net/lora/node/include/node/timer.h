/*
 / _____)             _              | |
((____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____)) ____| | | || |_| ____((___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Timer objects and scheduling management

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#ifndef __TIMER_H__
#define __TIMER_H__

#include <inttypes.h>
#include <stdbool.h>
struct hal_timer;

/*!
 * \brief Return the Time elapsed since a fix moment in Time
 *
 * \param [IN] savedTime    fix moment in Time
 * \retval time             returns elapsed time
 */
uint32_t TimerGetElapsedTime(uint32_t savedTime);

/*!
 * \brief Return the Time elapsed since a fix moment in Time
 *
 * \param [IN] eventInFuture    fix moment in the future
 * \retval time             returns difference between now and future event
 */
uint32_t TimerGetFutureTime(uint32_t eventInFuture);

#endif  // __TIMER_H__
