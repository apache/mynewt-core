 console\_set\_queues 
----------------------

.. code:: c

    void console_set_queues(struct os_eventq *avail, struct os_eventq *lines)

Sets the event queues that the console uses to manage input data
buffering and to send notification when a full line of data is received.

The caller must initialize the ``avail`` queue and add at least one
event to the queue. The event ``ev_cb`` field should point to the
callback function to call when a full line of data is received, and the
event ``ev_arg`` should point to a ``console_input`` structure.

The lines queue can be the OS default event queue or an event queue that
the caller has initialized. If the OS default event queue is specified,
events from this queue are processed in the context of the OS main task.
If a different event queue is specified, the call must initialize the
event queue and a task to process events from the queue.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| ``avail``    | Event queue to |
|              | queue          |
|              | available      |
|              | buffer events. |
|              | An event       |
|              | indicates that |
|              | the console    |
|              | can use the    |
|              | buffer from    |
|              | event data to  |
|              | buffer input   |
|              | data until a   |
|              | new line is    |
|              | received.      |
+--------------+----------------+
| ``lines``    | Event queue to |
|              | queue full     |
|              | line events.   |
|              | An event       |
|              | indicates that |
|              | a full line of |
|              | input is       |
|              | received and   |
|              | the event      |
|              | callback       |
|              | function can   |
|              | be called to   |
|              | process the    |
|              | input line.    |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

None

Example
^^^^^^^

.. code:: c


    #define MAX_CONSOLE_LINES 2

    static void myapp_process_input(struct os_event *ev);

    static struct os_eventq avail_queue;

    static void myapp_process_input(struct os_event *ev);

    static struct console_input myapp_console_buf;

    static struct console_input myapp_console_buf[MAX_CONSOLE_LINES];
    static struct os_event myapp_console_event[MAX_CONSOLE_LINES];


    int
    main(int argc, char **argv)
    {
         int i;
         
         os_eventq_init(&avail_queue);

         for (i = 0; i < MAX_CONSOLE_LINES; i++) {
             myapp_console_event[i].ev_cb = myapp_process_input;
             myapp_console_event[i].ev_arg = &myapp_console_buf[i]; 
             os_eventq_put(&avail_queue, &myapp_console_event[i]);
         }

         console_set_queues(&avail_queue, os_eventq_dflt_get());
    }

