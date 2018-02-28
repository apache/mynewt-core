Watchdog
=============

The hardware independent interface to enable internal hardware
watchdogs.

Description
~~~~~~~~~~~

The ``hal_watchdog_init`` interface can be used to set a recurring
watchdog timer to fire no sooner than in 'expire\_secs' seconds.

.. code:: c

    int hal_watchdog_init(uint32_t expire_msecs);

Watchdog needs to be then started with a call to
``hal_watchdog_enable()``. Watchdog should be tickled periodically with
a frequency smaller than 'expire\_secs' using ``hal_watchdog_tickle()``.

API
~~~~~~~~

.. doxygengroup:: HALWatchdog
    :content-only:
    :members:
