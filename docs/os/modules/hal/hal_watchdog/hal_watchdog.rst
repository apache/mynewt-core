hal\_watchdog
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

Definition
~~~~~~~~~~

`hal\_watchdog <https://github.com/apache/incubator-mynewt-core/blob/master/hw/hal/include/hal/hal_watchdog.h>`__

Examples
~~~~~~~~

The OS initializes and starts a watchdog timer and tickles it
periodically to check that the OS is running properly. This can be seen
in
`/kernel/os/src/os.c <https://github.com/apache/incubator-mynewt-core/blob/master/kernel/os/src/os.c>`__.
