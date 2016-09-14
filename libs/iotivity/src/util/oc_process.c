/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Copyright (c) 2005, Swedish Institute of Computer Science
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
 */

#include "oc_process.h"
#include "oc_buffer.h"
#include <stdio.h>

/*
 * Pointer to the currently running process structure.
 */
struct oc_process *oc_process_list = NULL;
struct oc_process *oc_process_current = NULL;

static oc_process_event_t lastevent;

/*
 * Structure used for keeping the queue of active events.
 */
struct event_data
{
  oc_process_event_t ev;
  oc_process_data_t data;
  struct oc_process *p;
};

static oc_process_num_events_t nevents, fevent;
static struct event_data events[OC_PROCESS_CONF_NUMEVENTS];

#if OC_PROCESS_CONF_STATS
oc_process_num_events_t process_maxevents;
#endif

static volatile unsigned char poll_requested;

#define OC_PROCESS_STATE_NONE 0
#define OC_PROCESS_STATE_RUNNING 1
#define OC_PROCESS_STATE_CALLED 2

static void call_process(struct oc_process *p, oc_process_event_t ev,
                         oc_process_data_t data);

/*---------------------------------------------------------------------------*/
oc_process_event_t
oc_process_alloc_event(void)
{
  return lastevent++;
}
/*---------------------------------------------------------------------------*/
void
oc_process_start(struct oc_process *p, oc_process_data_t data)
{
  struct oc_process *q;

  /* First make sure that we don't try to start a process that is
     already running. */
  for (q = oc_process_list; q != p && q != NULL; q = q->next)
    ;

  /* If we found the process on the process list, we bail out. */
  if (q == p) {
    return;
  }
  /* Put on the procs list.*/
  p->next = oc_process_list;
  oc_process_list = p;
  p->state = OC_PROCESS_STATE_RUNNING;
  PT_INIT(&p->pt);

  /* Post a synchronous initialization event to the process. */
  oc_process_post_synch(p, OC_PROCESS_EVENT_INIT, data);
}
/*---------------------------------------------------------------------------*/
static void
exit_process(struct oc_process *p, struct oc_process *fromprocess)
{
  register struct oc_process *q;
  struct oc_process *old_current = oc_process_current;

  /* Make sure the process is in the process list before we try to
     exit it. */
  for (q = oc_process_list; q != p && q != NULL; q = q->next)
    ;
  if (q == NULL) {
    return;
  }

  if (oc_process_is_running(p)) {
    /* Process was running */
    p->state = OC_PROCESS_STATE_NONE;

    /*
     * Post a synchronous event to all processes to inform them that
     * this process is about to exit. This will allow services to
     * deallocate state associated with this process.
     */
    for (q = oc_process_list; q != NULL; q = q->next) {
      if (p != q) {
        call_process(q, OC_PROCESS_EVENT_EXITED, (oc_process_data_t)p);
      }
    }

    if (p->thread != NULL && p != fromprocess) {
      /* Post the exit event to the process that is about to exit. */
      oc_process_current = p;
      p->thread(&p->pt, OC_PROCESS_EVENT_EXIT, NULL);
    }
  }

  if (p == oc_process_list) {
    oc_process_list = oc_process_list->next;
  } else {
    for (q = oc_process_list; q != NULL; q = q->next) {
      if (q->next == p) {
        q->next = p->next;
        break;
      }
    }
  }

  oc_process_current = old_current;
}
/*---------------------------------------------------------------------------*/
static void
call_process(struct oc_process *p, oc_process_event_t ev,
             oc_process_data_t data)
{
  int ret;

  if ((p->state & OC_PROCESS_STATE_RUNNING) && p->thread != NULL) {
    oc_process_current = p;
    p->state = OC_PROCESS_STATE_CALLED;
    ret = p->thread(&p->pt, ev, data);
    if (ret == PT_EXITED || ret == PT_ENDED || ev == OC_PROCESS_EVENT_EXIT) {
      exit_process(p, p);
    } else {
      p->state = OC_PROCESS_STATE_RUNNING;
    }
  }
}
/*---------------------------------------------------------------------------*/
void
oc_process_exit(struct oc_process *p)
{
  exit_process(p, OC_PROCESS_CURRENT());
}
/*---------------------------------------------------------------------------*/
void
oc_process_init(void)
{
  lastevent = OC_PROCESS_EVENT_MAX;

  nevents = fevent = 0;
#if OC_PROCESS_CONF_STATS
  process_maxevents = 0;
#endif /* OC_PROCESS_CONF_STATS */

  oc_process_current = oc_process_list = NULL;
}
/*---------------------------------------------------------------------------*/
/*
 * Call each process' poll handler.
 */
/*---------------------------------------------------------------------------*/
static void
do_poll(void)
{
  struct oc_process *p;

  poll_requested = 0;
  /* Call the processes that needs to be polled. */
  for (p = oc_process_list; p != NULL; p = p->next) {
    if (p->needspoll) {
      p->state = OC_PROCESS_STATE_RUNNING;
      p->needspoll = 0;
      call_process(p, OC_PROCESS_EVENT_POLL, NULL);
    }
  }
}
/*---------------------------------------------------------------------------*/
/*
 * Process the next event in the event queue and deliver it to
 * listening processes.
 */
/*---------------------------------------------------------------------------*/
static void
do_event(void)
{
  static oc_process_event_t ev;
  static oc_process_data_t data;
  static struct oc_process *receiver;
  static struct oc_process *p;

  /*
   * If there are any events in the queue, take the first one and walk
   * through the list of processes to see if the event should be
   * delivered to any of them. If so, we call the event handler
   * function for the process. We only process one event at a time and
   * call the poll handlers inbetween.
   */

  if (nevents > 0) {

    /* There are events that we should deliver. */
    ev = events[fevent].ev;

    data = events[fevent].data;
    receiver = events[fevent].p;

    /* Since we have seen the new event, we move pointer upwards
       and decrease the number of events. */
    fevent = (fevent + 1) % OC_PROCESS_CONF_NUMEVENTS;
    --nevents;

    /* If this is a broadcast event, we deliver it to all events, in
       order of their priority. */
    if (receiver == OC_PROCESS_BROADCAST) {
      for (p = oc_process_list; p != NULL; p = p->next) {

        /* If we have been requested to poll a process, we do this in
           between processing the broadcast event. */
        if (poll_requested) {
          do_poll();
        }
        call_process(p, ev, data);
      }
    } else {
      /* This is not a broadcast event, so we deliver it to the
   specified process. */
      /* If the event was an INIT event, we should also update the
   state of the process. */
      if (ev == OC_PROCESS_EVENT_INIT) {
        receiver->state = OC_PROCESS_STATE_RUNNING;
      }

      /* Make sure that the process actually is running. */
      call_process(receiver, ev, data);
    }
  }
}
/*---------------------------------------------------------------------------*/
int
oc_process_run(void)
{
  /* Process poll events. */
  if (poll_requested) {
    do_poll();
  }

  /* Process one event from the queue */
  do_event();

  return nevents + poll_requested;
}
/*---------------------------------------------------------------------------*/
int
oc_process_nevents(void)
{
  return nevents + poll_requested;
}
/*---------------------------------------------------------------------------*/
int
oc_process_post(struct oc_process *p, oc_process_event_t ev,
                oc_process_data_t data)
{
  static oc_process_num_events_t snum;

  if (nevents == OC_PROCESS_CONF_NUMEVENTS) {
    oc_message_unref(data);
    return OC_PROCESS_ERR_FULL;
  }

  snum =
    (oc_process_num_events_t)(fevent + nevents) % OC_PROCESS_CONF_NUMEVENTS;
  events[snum].ev = ev;
  events[snum].data = data;
  events[snum].p = p;
  ++nevents;

#if OC_PROCESS_CONF_STATS
  if (nevents > process_maxevents) {
    process_maxevents = nevents;
  }
#endif /* OC_PROCESS_CONF_STATS */

  return OC_PROCESS_ERR_OK;
}
/*---------------------------------------------------------------------------*/
void
oc_process_post_synch(struct oc_process *p, oc_process_event_t ev,
                      oc_process_data_t data)
{
  struct oc_process *caller = oc_process_current;

  call_process(p, ev, data);
  oc_process_current = caller;
}
/*---------------------------------------------------------------------------*/
void
oc_process_poll(struct oc_process *p)
{
  if (p != NULL) {
    if (p->state == OC_PROCESS_STATE_RUNNING ||
        p->state == OC_PROCESS_STATE_CALLED) {
      p->needspoll = 1;
      poll_requested = 1;
    }
  }
}
/*---------------------------------------------------------------------------*/
int
oc_process_is_running(struct oc_process *p)
{
  return p->state != OC_PROCESS_STATE_NONE;
}
/*---------------------------------------------------------------------------*/
