Callout
=======

Callouts are MyNewt OS timers.

Description
~~~~~~~~~~~

Callout is a way of setting up an OS timer. When the timer fires, it is
delivered as an event to task's event queue.

User would initialize their callout structure using
*os\_callout\_init()*, or *os\_callout\_func\_init()* and then arm it
with *os\_callout\_reset()*.

If user wants to cancel the timer before it expires, they can either use
*os\_callout\_reset()* to arm it for later expiry, or stop it altogether
by calling *os\_callout\_stop()*.

There are 2 different options for data structure to use. First is
*struct os\_callout*, which is a bare-bones version. You would
initialize this with *os\_callout\_init()*.

Second option is *struct os\_callout\_func*. This you can use if you
expect to have multiple different types of timers in your task, running
concurrently. The structure contains a function pointer, and you would
call that function from your task's event processing loop.

Time unit when arming the timer is OS ticks. This rate of this ticker
depends on the platform this is running on. You should use OS define
*OS\_TICKS\_PER\_SEC* to convert wallclock time to OS ticks.

Callout timer fires out just once. For periodic timer type of operation
you need to rearm it once it fires.

Data structures
~~~~~~~~~~~~~~~

::

    struct os_callout {
        struct os_event c_ev;
        struct os_eventq *c_evq;
        uint32_t c_ticks;
        TAILQ_ENTRY(os_callout) c_next;
    };

+------------+------------------------------------------------------------+
| Element    | Description                                                |
+============+============================================================+
| c\_ev      | Event structure of this callout                            |
+------------+------------------------------------------------------------+
| c\_evq     | Event queue where this callout is placed on timer expiry   |
+------------+------------------------------------------------------------+
| c\_ticks   | OS tick amount when timer fires                            |
+------------+------------------------------------------------------------+
| c\_next    | Linkage to other unexpired callouts                        |
+------------+------------------------------------------------------------+

::

    struct os_callout_func {
        struct os_callout cf_c;
        os_callout_func_t cf_func;
        void *cf_arg;
    };

+------------+---------------------------------------------------------------------+
| Element    | Description                                                         |
+============+=====================================================================+
| cf\_c      | struct os\_callout. See above                                       |
+------------+---------------------------------------------------------------------+
| cf\_func   | Function pointer which should be called by event queue processing   |
+------------+---------------------------------------------------------------------+
| cf\_arg    | Generic void \* argument to that function                           |
+------------+---------------------------------------------------------------------+

API
-----------------

.. doxygengroup:: OSCallouts
    :content-only:
