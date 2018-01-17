CPU Time
========

The MyNewt ``cputime`` module provides high resolution time and timer
support.

Description
-----------

The ``cputime`` API provides high resolution time and timer support. The
module must be initialized, using the ``os_cputime_init()`` function,
with the clock frequency to use. The module uses the ``hal_timer`` API,
defined in hal/hal_timer.h, to access the hardware timers. It uses the
hardware timer number specified by the ``OS_CPUTIME_TIMER_NUM`` system
configuration setting.

Data Structures
---------------

The module uses the following data structures:

-  ``uint32_t`` to represent ``cputime``. Only the lower 32 bits of a 64
   timer are used.
-  ``struct hal_timer`` to represent a cputime timer.

API
-----------------

* :c:func:`os_cputime_delay_nsecs()`
* :c:func:`os_cputime_delay_ticks()`
* :c:func:`os_cputime_delay_usecs()`
* :c:func:`os_cputime_get32()`
* :c:func:`os_cputime_init()`
* :c:func:`os_cputime_nsecs_to_ticks()`
* :c:func:`os_cputime_ticks_to_nsecs()`
* :c:func:`os_cputime_ticks_to_usecs()`
* :c:func:`os_cputime_timer_init()`
* :c:func:`os_cputime_timer_relative()`
* :c:func:`os_cputime_timer_start()`
* :c:func:`os_cputime_timer_stop()`
* :c:func:`os_cputime_usecs_to_ticks()`
* :c:macro:`CPUTIME_LT`
* :c:macro:`CPUTIME_GT`
* :c:macro:`CPUTIME_LEQ`
* :c:macro:`CPUTIME_GEQ`
