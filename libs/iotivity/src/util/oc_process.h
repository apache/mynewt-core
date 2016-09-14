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

/**
 * \defgroup process Contiki processes
 *
 * A process in Contiki consists of a single \ref pt "protothread".
 *
 * @{
 */

#ifndef OC_PROCESS_H
#define OC_PROCESS_H
#include "util/pt/pt.h"

#ifndef NULL
#define NULL 0
#endif /* NULL */

typedef unsigned char oc_process_event_t;
typedef void *oc_process_data_t;
typedef unsigned char oc_process_num_events_t;

/**
 * \name Return values
 * @{
 */

/**
 * \brief      Return value indicating that an operation was successful.
 *
 *             This value is returned to indicate that an operation
 *             was successful.
 */
#define OC_PROCESS_ERR_OK 0
/**
 * \brief      Return value indicating that the event queue was full.
 *
 *             This value is returned from process_post() to indicate
 *             that the event queue was full and that an event could
 *             not be posted.
 */
#define OC_PROCESS_ERR_FULL 1
/* @} */

#define OC_PROCESS_NONE NULL

#ifndef OC_PROCESS_CONF_NUMEVENTS
#define OC_PROCESS_CONF_NUMEVENTS 10
#endif /* OC_PROCESS_CONF_NUMEVENTS */

#define OC_PROCESS_EVENT_NONE 0x80
#define OC_PROCESS_EVENT_INIT 0x81
#define OC_PROCESS_EVENT_POLL 0x82
#define OC_PROCESS_EVENT_EXIT 0x83
#define OC_PROCESS_EVENT_SERVICE_REMOVED 0x84
#define OC_PROCESS_EVENT_CONTINUE 0x85
#define OC_PROCESS_EVENT_MSG 0x86
#define OC_PROCESS_EVENT_EXITED 0x87
#define OC_PROCESS_EVENT_TIMER 0x88
#define OC_PROCESS_EVENT_COM 0x89
#define OC_PROCESS_EVENT_MAX 0x8a

#define OC_PROCESS_BROADCAST NULL
#define OC_PROCESS_ZOMBIE ((struct oc_process *)0x1)

/**
 * \name Process protothread functions
 * @{
 */

/**
 * Define the beginning of a process.
 *
 * This macro defines the beginning of a process, and must always
 * appear in a OC_PROCESS_THREAD() definition. The OC_PROCESS_END() macro
 * must come at the end of the process.
 *
 * \hideinitializer
 */
#define OC_PROCESS_BEGIN() PT_BEGIN(process_pt)

/**
 * Define the end of a process.
 *
 * This macro defines the end of a process. It must appear in a
 * OC_PROCESS_THREAD() definition and must always be included. The
 * process exits when the OC_PROCESS_END() macro is reached.
 *
 * \hideinitializer
 */
#define OC_PROCESS_END() PT_END(process_pt)

/**
 * Wait for an event to be posted to the process.
 *
 * This macro blocks the currently running process until the process
 * receives an event.
 *
 * \hideinitializer
 */
#define OC_PROCESS_WAIT_EVENT() OC_PROCESS_YIELD()

/**
 * Wait for an event to be posted to the process, with an extra
 * condition.
 *
 * This macro is similar to OC_PROCESS_WAIT_EVENT() in that it blocks the
 * currently running process until the process receives an event. But
 * OC_PROCESS_WAIT_EVENT_UNTIL() takes an extra condition which must be
 * true for the process to continue.
 *
 * \param c The condition that must be true for the process to continue.
 * \sa PT_WAIT_UNTIL()
 *
 * \hideinitializer
 */
#define OC_PROCESS_WAIT_EVENT_UNTIL(c) OC_PROCESS_YIELD_UNTIL(c)

/**
 * Yield the currently running process.
 *
 * \hideinitializer
 */
#define OC_PROCESS_YIELD() PT_YIELD(process_pt)

/**
 * Yield the currently running process until a condition occurs.
 *
 * This macro is different from OC_PROCESS_WAIT_UNTIL() in that
 * OC_PROCESS_YIELD_UNTIL() is guaranteed to always yield at least
 * once. This ensures that the process does not end up in an infinite
 * loop and monopolizing the CPU.
 *
 * \param c The condition to wait for.
 *
 * \hideinitializer
 */
#define OC_PROCESS_YIELD_UNTIL(c) PT_YIELD_UNTIL(process_pt, c)

/**
 * Wait for a condition to occur.
 *
 * This macro does not guarantee that the process yields, and should
 * therefore be used with care. In most cases, OC_PROCESS_WAIT_EVENT(),
 * OC_PROCESS_WAIT_EVENT_UNTIL(), OC_PROCESS_YIELD() or
 * OC_PROCESS_YIELD_UNTIL() should be used instead.
 *
 * \param c The condition to wait for.
 *
 * \hideinitializer
 */
#define OC_PROCESS_WAIT_UNTIL(c) PT_WAIT_UNTIL(process_pt, c)
#define OC_PROCESS_WAIT_WHILE(c) PT_WAIT_WHILE(process_pt, c)

/**
 * Exit the currently running process.
 *
 * \hideinitializer
 */
#define OC_PROCESS_EXIT() PT_EXIT(process_pt)

/**
 * Spawn a protothread from the process.
 *
 * \param pt The protothread state (struct pt) for the new protothread
 * \param thread The call to the protothread function.
 * \sa PT_SPAWN()
 *
 * \hideinitializer
 */
#define OC_PROCESS_PT_SPAWN(pt, thread) PT_SPAWN(process_pt, pt, thread)

/**
 * Yield the process for a short while.
 *
 * This macro yields the currently running process for a short while,
 * thus letting other processes run before the process continues.
 *
 * \hideinitializer
 */
#define OC_PROCESS_PAUSE()                                                     \
  do {                                                                         \
    process_post(OC_PROCESS_CURRENT(), OC_PROCESS_EVENT_CONTINUE, NULL);       \
    OC_PROCESS_WAIT_EVENT_UNTIL(ev == OC_PROCESS_EVENT_CONTINUE);              \
  } while (0)

/** @} end of protothread functions */

/**
 * \name Poll and exit handlers
 * @{
 */
/**
 * Specify an action when a process is polled.
 *
 * \note This declaration must come immediately before the
 * OC_PROCESS_BEGIN() macro.
 *
 * \param handler The action to be performed.
 *
 * \hideinitializer
 */
#define OC_PROCESS_POLLHANDLER(handler)                                        \
  if (ev == OC_PROCESS_EVENT_POLL) {                                           \
    handler;                                                                   \
  }

/**
 * Specify an action when a process exits.
 *
 * \note This declaration must come immediately before the
 * OC_PROCESS_BEGIN() macro.
 *
 * \param handler The action to be performed.
 *
 * \hideinitializer
 */
#define OC_PROCESS_EXITHANDLER(handler)                                        \
  if (ev == OC_PROCESS_EVENT_EXIT) {                                           \
    handler;                                                                   \
  }

/** @} */

/**
 * \name Process declaration and definition
 * @{
 */

/**
 * Define the body of a process.
 *
 * This macro is used to define the body (protothread) of a
 * process. The process is called whenever an event occurs in the
 * system, A process always start with the OC_PROCESS_BEGIN() macro and
 * end with the OC_PROCESS_END() macro.
 *
 * \hideinitializer
 */
#define OC_PROCESS_THREAD(name, ev, data)                                      \
  static PT_THREAD(process_thread_##name(                                      \
    struct pt *process_pt, oc_process_event_t ev, oc_process_data_t data))

/**
 * Declare the name of a process.
 *
 * This macro is typically used in header files to declare the name of
 * a process that is implemented in the C file.
 *
 * \hideinitializer
 */
#define OC_PROCESS_NAME(name) extern struct oc_process name

/**
 * Declare a process.
 *
 * This macro declares a process. The process has two names: the
 * variable of the process structure, which is used by the C program,
 * and a human readable string name, which is used when debugging.
 * A configuration option allows removal of the readable name to save RAM.
 *
 * \param name The variable name of the process structure.
 * \param strname The string representation of the process' name.
 *
 * \hideinitializer
 */
#if OC_PROCESS_CONF_NO_OC_PROCESS_NAMES
#define OC_PROCESS(name, strname)                                              \
  OC_PROCESS_THREAD(name, ev, data);                                           \
  struct oc_process name = { NULL, process_thread_##name }
#else
#define OC_PROCESS(name, strname)                                              \
  OC_PROCESS_THREAD(name, ev, data);                                           \
  struct oc_process name = { NULL, strname, process_thread_##name }
#endif

/** @} */

struct oc_process
{
  struct oc_process *next;
#if OC_PROCESS_CONF_NO_OC_PROCESS_NAMES
#define OC_PROCESS_NAME_STRING(process) ""
#else
  const char *name;
#define OC_PROCESS_NAME_STRING(process) (process)->name
#endif
  PT_THREAD((*thread)(struct pt *, oc_process_event_t, oc_process_data_t));
  struct pt pt;
  unsigned char state, needspoll;
};

/**
 * \name Functions called from application programs
 * @{
 */

/**
 * Start a process.
 *
 * \param p A pointer to a process structure.
 *
 * \param data An argument pointer that can be passed to the new
 * process
 *
 */
void oc_process_start(struct oc_process *p, oc_process_data_t data);

/**
 * Post an asynchronous event.
 *
 * This function posts an asynchronous event to one or more
 * processes. The handing of the event is deferred until the target
 * process is scheduled by the kernel. An event can be broadcast to
 * all processes, in which case all processes in the system will be
 * scheduled to handle the event.
 *
 * \param ev The event to be posted.
 *
 * \param data The auxiliary data to be sent with the event
 *
 * \param p The process to which the event should be posted, or
 * OC_PROCESS_BROADCAST if the event should be posted to all processes.
 *
 * \retval OC_PROCESS_ERR_OK The event could be posted.
 *
 * \retval OC_PROCESS_ERR_FULL The event queue was full and the event could
 * not be posted.
 */
int oc_process_post(struct oc_process *p, oc_process_event_t ev,
                    oc_process_data_t data);

/**
 * Post a synchronous event to a process.
 *
 * \param p A pointer to the process' process structure.
 *
 * \param ev The event to be posted.
 *
 * \param data A pointer to additional data that is posted together
 * with the event.
 */
void oc_process_post_synch(struct oc_process *p, oc_process_event_t ev,
                           oc_process_data_t data);

/**
 * \brief      Cause a process to exit
 * \param p    The process that is to be exited
 *
 *             This function causes a process to exit. The process can
 *             either be the currently executing process, or another
 *             process that is currently running.
 *
 * \sa OC_PROCESS_CURRENT()
 */
void oc_process_exit(struct oc_process *p);

/**
 * Get a pointer to the currently running process.
 *
 * This macro get a pointer to the currently running
 * process. Typically, this macro is used to post an event to the
 * current process with process_post().
 *
 * \hideinitializer
 */
#define OC_PROCESS_CURRENT() oc_process_current
extern struct oc_process *oc_process_current;

/**
 * Switch context to another process
 *
 * This function switch context to the specified process and executes
 * the code as if run by that process. Typical use of this function is
 * to switch context in services, called by other processes. Each
 * OC_PROCESS_CONTEXT_BEGIN() must be followed by the
 * OC_PROCESS_CONTEXT_END() macro to end the context switch.
 *
 * Example:
 \code
 OC_PROCESS_CONTEXT_BEGIN(&test_process);
 etimer_set(&timer, CLOCK_SECOND);
 OC_PROCESS_CONTEXT_END(&test_process);
 \endcode
 *
 * \param p    The process to use as context
 *
 * \sa OC_PROCESS_CONTEXT_END()
 * \sa OC_PROCESS_CURRENT()
 */
#define OC_PROCESS_CONTEXT_BEGIN(p)                                            \
  {                                                                            \
    struct oc_process *tmp_current = OC_PROCESS_CURRENT();                     \
  oc_process_current = p

/**
 * End a context switch
 *
 * This function ends a context switch and changes back to the
 * previous process.
 *
 * \param p    The process used in the context switch
 *
 * \sa OC_PROCESS_CONTEXT_START()
 */
#define OC_PROCESS_CONTEXT_END(p)                                              \
  oc_process_current = tmp_current;                                            \
  }

/**
 * \brief      Allocate a global event number.
 * \return     The allocated event number
 *
 *             In Contiki, event numbers above 128 are global and may
 *             be posted from one process to another. This function
 *             allocates one such event number.
 *
 * \note       There currently is no way to deallocate an allocated event
 *             number.
 */
oc_process_event_t oc_process_alloc_event(void);

/** @} */

/**
 * \name Functions called from device drivers
 * @{
 */

/**
 * Request a process to be polled.
 *
 * This function typically is called from an interrupt handler to
 * cause a process to be polled.
 *
 * \param p A pointer to the process' process structure.
 */
void oc_process_poll(struct oc_process *p);

/** @} */

/**
 * \name Functions called by the system and boot-up code
 * @{
 */

/**
 * \brief      Initialize the process module.
 *
 *             This function initializes the process module and should
 *             be called by the system boot-up code.
 */
void oc_process_init(void);

/**
 * Run the system once - call poll handlers and process one event.
 *
 * This function should be called repeatedly from the main() program
 * to actually run the Contiki system. It calls the necessary poll
 * handlers, and processes one event. The function returns the number
 * of events that are waiting in the event queue so that the caller
 * may choose to put the CPU to sleep when there are no pending
 * events.
 *
 * \return The number of events that are currently waiting in the
 * event queue.
 */
int oc_process_run(void);

/**
 * Check if a process is running.
 *
 * This function checks if a specific process is running.
 *
 * \param p The process.
 * \retval Non-zero if the process is running.
 * \retval Zero if the process is not running.
 */
int oc_process_is_running(struct oc_process *p);

/**
 *  Number of events waiting to be processed.
 *
 * \return The number of events that are currently waiting to be
 * processed.
 */
int oc_process_nevents(void);

/** @} */

extern struct oc_process *oc_process_list;

#define OC_PROCESS_LIST() oc_process_list

#endif /* OC_PROCESS_H */
