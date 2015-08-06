/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef H_HAL_CPUTIME_
#define H_HAL_CPUTIME_

#include "os/queue.h"

/* CPU timer callback function */
struct cpu_timer;
typedef void (*cputimer_func)(void *arg);

/* CPU timer */
struct cpu_timer
{
    cputimer_func   cb;
    void            *arg;
    uint32_t        cputime;
    TAILQ_ENTRY(cpu_timer) link;
};

int cputime_init(uint32_t clock_freq);
uint64_t cputime_get(void);
uint32_t cputime_low(void);
uint32_t cputime_nsecs_to_ticks(uint32_t nsecs);
uint32_t cputime_ticks_to_nsecs(uint32_t ticks);
uint32_t cputime_usecs_to_ticks(uint32_t usecs);
uint32_t cputime_ticks_to_usecs(uint32_t ticks);
void cputime_delay_nsecs(uint32_t nsec_delay);
void cputime_delay_usecs(uint32_t delay_usecs);
void cputime_delay_ticks(uint32_t ticks);
void cputime_timer_init(struct cpu_timer *timer, cputimer_func fp, void *arg);
void cputime_timer_start(struct cpu_timer *timer, uint32_t cputime);
void cputime_timer_relative(struct cpu_timer *timer, uint32_t usecs);
void cputime_timer_stop(struct cpu_timer *timer);

#define CPUTIME_LT(__t1, __t2) ((int32_t)   ((__t1) - (__t2)) < 0)
#define CPUTIME_GT(__t1, __t2) ((int32_t)   ((__t1) - (__t2)) > 0)
#define CPUTIME_GEQ(__t1, __t2) ((int32_t)  ((__t1) - (__t2)) >= 0)
#define CPUTIME_LEQ(__t1, __t2) ((int32_t)  ((__t1) - (__t2)) <= 0)

#endif /* H_HAL_CPUTIMER_ */
