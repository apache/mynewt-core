Callout
=======

Callouts are Apache Mynewt OS timers.

Description
~~~~~~~~~~~

Callout is a way of setting up an OS timer. When the timer fires, it is
delivered as an event to task's event queue.

User would initialize their callout structure :c:type:`struct os_callout` using
:c:func:`os_callout_init()` and then arm it with :c:func:`os_callout_reset()`.

If user wants to cancel the timer before it expires, they can either use
:c:func:`os_callout_reset()` to arm it for later expiry, or stop it altogether
by calling :c:func:`os_callout_stop()`.

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
