Callout
=======

Callouts are Apache Mynewt OS timers.

Description
~~~~~~~~~~~

Callout is a way of setting up an OS timer. When the timer fires, it is
delivered as an event to task's event queue.

User would initialize their callout structure using
:c:func:`os_callout_init()`, or :c:func:`os_callout_func_init()` and 
then arm it with :c:func:`os_callout_reset()`.

If user wants to cancel the timer before it expires, they can either use
:c:func:`os_callout_reset()` to arm it for later expiry, or stop it altogether
by calling :c:func:`os_callout_stop()`.

There are 2 different options for data structure to use. First is
:c:type:`struct os_callout`, which is a bare-bones version. You would
initialize this with :c:func:`os_callout_init()`.

Second option is :c:type:`struct os_callout_func`. This you can use if you
expect to have multiple different types of timers in your task, running
concurrently. The structure contains a function pointer, and you would
call that function from your task's event processing loop.

Time unit when arming the timer is OS ticks. This rate of this ticker
depends on the platform this is running on. You should use OS define
``OS_TICKS_PER_SEC`` to convert wallclock time to OS ticks.

Callout timer fires out just once. For periodic timer type of operation
you need to rearm it once it fires.


API
-----------------

.. doxygengroup:: OSCallouts
    :content-only:
    :members:
