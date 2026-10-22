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

#ifndef H_STATIC_TASK_
#define H_STATIC_TASK_

#include "os/os.h"
#include "os/link_tables.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tasks defined with STATIC_TASK_DEFINE() or STATIC_EVENTQ_TASK_DEFINE()
 * are collected in the static_task_defs link table and started by
 * static_task_init() at sysinit stage STATIC_TASK_SYSINIT_STAGE, after
 * all packages are initialized.
 *
 * - Task name is the stringified name argument; symbols defined are
 *   global so duplicate names fail to link.
 * - Tasks are started in alphabetical order of their names.
 * - Stack size is in os_stack_t units, as for OS_TASK_STACK_DEFINE().
 * - Definition is only linked if its object file is pulled in from the
 *   package archive.  This is normally the case since the file also has
 *   code referenced from elsewhere (e.g. users of <name>_evq or the
 *   package init function); otherwise set pkg.whole_archive: true.
 *
 * Example:
 *
 * STATIC_EVENTQ_TASK_DEFINE(foo, MYNEWT_VAL(FOO_TASK_PRIO),
 *                           MYNEWT_VAL(FOO_TASK_STACK_SIZE));
 *
 * os_eventq_put(&foo_evq, &foo_event);
 */

/** Static definition of task, created by static_task_init(). */
struct static_task_def {
    /** Task to initialize */
    struct os_task *task;
    /** Task name */
    const char *name;
    /** Task handler */
    os_task_func_t func;
    /** Argument passed to task handler */
    void *arg;
    /** Bottom of task stack */
    os_stack_t *stack;
    /** Task stack size in os_stack_t units */
    uint16_t stack_size;
    /** Task priority */
    uint8_t prio;
};

/*
 * Table holds pointers rather than structures so element size does not
 * depend on how the compiler aligns structures placed in sections.
 */
LINK_TABLE(const struct static_task_def *, static_task_defs)

/**
 * Defines task <name>_task with stack <name>_stack and adds it to the
 * list of tasks started by static_task_init().
 *
 * @param _name                 Task name, also prefix of defined symbols
 * @param _prio                 Task priority
 * @param _stack_size           Stack size in os_stack_t units
 * @param _func                 Task handler
 * @param _arg                  Argument passed to task handler
 */
#define STATIC_TASK_DEFINE(_name, _prio, _stack_size, _func, _arg)            \
    struct os_task _name##_task;                                              \
    OS_TASK_STACK_DEFINE(_name##_stack, _stack_size)                          \
    static const struct static_task_def _name##_task_def = {                  \
        .task = &_name##_task,                                                \
        .name = #_name,                                                       \
        .func = (_func),                                                      \
        .arg = (_arg),                                                        \
        .stack = _name##_stack,                                               \
        .stack_size = OS_STACK_ALIGN(_stack_size),                            \
        .prio = (_prio),                                                      \
    };                                                                        \
    LINK_TABLE_ELEMENT(static_task_defs, _name##_task_def_ptr) =              \
        &_name##_task_def

/**
 * Defines task <name>_task with stack <name>_stack and event queue
 * <name>_evq.  The task processes events from <name>_evq forever.
 * The event queue is statically initialized, events can be put on it
 * before the task is started.
 *
 * @param _name                 Task name, also prefix of defined symbols
 * @param _prio                 Task priority
 * @param _stack_size           Stack size in os_stack_t units
 */
#define STATIC_EVENTQ_TASK_DEFINE(_name, _prio, _stack_size)                  \
    struct os_eventq _name##_evq = OS_EVENTQ_INITIALIZER(_name##_evq);        \
    STATIC_TASK_DEFINE(_name, _prio, _stack_size, os_eventq_task_handler,     \
                       &_name##_evq)

/**
 * Declares symbols defined by STATIC_EVENTQ_TASK_DEFINE() for use in
 * other files.
 *
 * @param _name                 Name used in STATIC_EVENTQ_TASK_DEFINE()
 */
#define STATIC_EVENTQ_TASK_DECLARE(_name)                                     \
    extern struct os_task _name##_task;                                       \
    extern struct os_eventq _name##_evq

/**
 * Starts all tasks defined with STATIC_TASK_DEFINE() and
 * STATIC_EVENTQ_TASK_DEFINE().  Called by sysinit.
 */
void static_task_init(void);

#ifdef __cplusplus
}
#endif

#endif /* H_STATIC_TASK_ */
