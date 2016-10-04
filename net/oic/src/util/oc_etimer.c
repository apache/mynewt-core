/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Copyright (c) 2004, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "oc_etimer.h"
#include "oc_process.h"

static struct oc_etimer *timerlist;
static oc_clock_time_t next_expiration;

OC_PROCESS(oc_etimer_process, "Event timer");
/*---------------------------------------------------------------------------*/
static void
update_time(void)
{
  oc_clock_time_t tdist;
  oc_clock_time_t now;
  struct oc_etimer *t;

  if (timerlist == NULL) {
    next_expiration = 0;
  } else {
    now = oc_clock_time();
    t = timerlist;
    /* Must calculate distance to next time into account due to wraps */
    tdist = t->timer.start + t->timer.interval - now;
    for (t = t->next; t != NULL; t = t->next) {
      if (t->timer.start + t->timer.interval - now < tdist) {
        tdist = t->timer.start + t->timer.interval - now;
      }
    }
    next_expiration = now + tdist;
  }
}
/*---------------------------------------------------------------------------*/
OC_PROCESS_THREAD(oc_etimer_process, ev, data)
{
  struct oc_etimer *t, *u;

  OC_PROCESS_BEGIN();

  timerlist = NULL;

  while (1) {
    OC_PROCESS_YIELD();

    if (ev == OC_PROCESS_EVENT_EXITED) {
      struct oc_process *p = data;

      while (timerlist != NULL && timerlist->p == p) {
        timerlist = timerlist->next;
      }

      if (timerlist != NULL) {
        t = timerlist;
        while (t->next != NULL) {
          if (t->next->p == p) {
            t->next = t->next->next;
          } else
            t = t->next;
        }
      }
      continue;
    } else if (ev != OC_PROCESS_EVENT_POLL) {
      continue;
    }

  again:

    u = NULL;

    for (t = timerlist; t != NULL; t = t->next) {
      if (oc_timer_expired(&t->timer)) {
        if (oc_process_post(t->p, OC_PROCESS_EVENT_TIMER, t) ==
            OC_PROCESS_ERR_OK) {

          /* Reset the process ID of the event timer, to signal that the
             etimer has expired. This is later checked in the
             oc_etimer_expired() function. */
          t->p = OC_PROCESS_NONE;
          if (u != NULL) {
            u->next = t->next;
          } else {
            timerlist = t->next;
          }
          t->next = NULL;
          update_time();
          goto again;
        } else {
          oc_etimer_request_poll();
        }
      }
      u = t;
    }
  }

  OC_PROCESS_END();
}
/*---------------------------------------------------------------------------*/
oc_clock_time_t
oc_etimer_request_poll(void)
{
  oc_process_poll(&oc_etimer_process);
  return oc_etimer_next_expiration_time();
}
/*---------------------------------------------------------------------------*/
static void
add_timer(struct oc_etimer *timer)
{
  struct oc_etimer *t;

  oc_etimer_request_poll();

  if (timer->p != OC_PROCESS_NONE) {
    for (t = timerlist; t != NULL; t = t->next) {
      if (t == timer) {
        /* Timer already on list, bail out. */
        timer->p = OC_PROCESS_CURRENT();
        update_time();
        return;
      }
    }
  }

  /* Timer not on list. */
  timer->p = OC_PROCESS_CURRENT();
  timer->next = timerlist;
  timerlist = timer;

  update_time();
}
/*---------------------------------------------------------------------------*/
void
oc_etimer_set(struct oc_etimer *et, oc_clock_time_t interval)
{
  oc_timer_set(&et->timer, interval);
  add_timer(et);
}
/*---------------------------------------------------------------------------*/
void
oc_etimer_reset_with_new_interval(struct oc_etimer *et,
                                  oc_clock_time_t interval)
{
  oc_timer_reset(&et->timer);
  et->timer.interval = interval;
  add_timer(et);
}
/*---------------------------------------------------------------------------*/
void
oc_etimer_reset(struct oc_etimer *et)
{
  oc_timer_reset(&et->timer);
  add_timer(et);
}
/*---------------------------------------------------------------------------*/
void
oc_etimer_restart(struct oc_etimer *et)
{
  oc_timer_restart(&et->timer);
  add_timer(et);
}
/*---------------------------------------------------------------------------*/
void
oc_etimer_adjust(struct oc_etimer *et, int timediff)
{
  et->timer.start += timediff;
  update_time();
}
/*---------------------------------------------------------------------------*/
int
oc_etimer_expired(struct oc_etimer *et)
{
  return et->p == OC_PROCESS_NONE;
}
/*---------------------------------------------------------------------------*/
oc_clock_time_t
oc_etimer_expiration_time(struct oc_etimer *et)
{
  return et->timer.start + et->timer.interval;
}
/*---------------------------------------------------------------------------*/
oc_clock_time_t
oc_etimer_start_time(struct oc_etimer *et)
{
  return et->timer.start;
}
/*---------------------------------------------------------------------------*/
int
oc_etimer_pending(void)
{
  return timerlist != NULL;
}
/*---------------------------------------------------------------------------*/
oc_clock_time_t
oc_etimer_next_expiration_time(void)
{
  return oc_etimer_pending() ? next_expiration : 0;
}
/*---------------------------------------------------------------------------*/
void
oc_etimer_stop(struct oc_etimer *et)
{
  struct oc_etimer *t;

  /* First check if et is the first event timer on the list. */
  if (et == timerlist) {
    timerlist = timerlist->next;
    update_time();
  } else {
    /* Else walk through the list and try to find the item before the
       et timer. */
    for (t = timerlist; t != NULL && t->next != et; t = t->next)
      ;

    if (t != NULL) {
      /* We've found the item before the event timer that we are about
   to remove. We point the items next pointer to the event after
   the removed item. */
      t->next = et->next;

      update_time();
    }
  }

  /* Remove the next pointer from the item to be removed. */
  et->next = NULL;
  /* Set the timer as expired */
  et->p = OC_PROCESS_NONE;
}
/*---------------------------------------------------------------------------*/
/** @} */
