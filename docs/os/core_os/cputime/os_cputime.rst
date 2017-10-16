CPU Time
========

The MyNewt ``cputime`` module provides high resolution time and timer
support.

Description
-----------

The ``cputime`` API provides high resolution time and timer support. The
module must be initialized, using the ``os_cputime_init()`` function,
with the clock frequency to use. The module uses the ``hal_timer`` API,
defined in hal/hal\_timer.h, to access the hardware timers. It uses the
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

.. doxygengroup:: OSCPUTime
    :content-only:

List of Macros
--------------

These macros should be used to evaluate the time with respect to each
other.

-  ``CPUIME_LT(t1,t2)`` -- evaluates to true if t1 is before t2 in time.
-  ``CPUTIME_GT(t1,t2)`` -- evaluates to true if t1 is after t2 in time
-  ``CPUTIME_LEQ(t1,t2)`` -- evaluates to true if t1 is on or before t2
   in time.
-  ``CPUTIME_GEQ(t1,t2)`` -- evaluates to true if t1 is on or after t2
   in time.
