
.. _cpu_time:

CPU Time
##################

.. toctree::
    :maxdepth: 1

    api/api.rst


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

List of Functions
-----------------

The functions available in cputime are:
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| **Function**                                                       | **Description**                                                                         |
+====================================================================+=========================================================================================+
| `os\_cputime\_delay\_nsecs <os_cputime_delay_nsecs.html>`__          | Delays for a specified number of nanoseconds.                                           |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_delay\_ticks <os_cputime_delay_ticks.html>`__          | Delays for a specified number of ticks.                                                 |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_delay\_usecs <os_cputime_delay_usecs.html>`__          | Delays for a specified number of microseconds.                                          |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_get32 <os_cputime_get32.html>`__                       | Gets the current value of the cpu time.                                                 |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_init <os_cputime_init.html>`__                         | Initializes the cputime module.                                                         |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_nsecs\_to\_ticks <os_cputime_nsecs_to_ticks.html>`__   | Converts the specified number of nanoseconds to number of ticks.                        |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_ticks\_to\_nsecs <os_cputime_ticks_to_nsecs.html>`__   | Converts the specified number of ticks to number of nanoseconds.                        |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_ticks\_to\_usecs <os_cputime_ticks_to_usecs.html>`__   | Converts the specified number of ticks to number of microseconds.                       |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_timer\_init <os_cputime_timer_init.html>`__            | Initializes a timer.                                                                    |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_timer\_relative <os_cputime_timer_relative.html>`__    | Sets a timer to expire in the specified number of microseconds from the current time.   |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_timer\_start <os_cputime_timer_start.html>`__          | Sets a timer to expire at the specified cputime.                                        |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_timer\_stop <os_cputime_timer_stop.html>`__            | Stops a timer from running.                                                             |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+
| `os\_cputime\_usecs\_to\_ticks <os_cputime_usecs_to_ticks.html>`__   | Converts the specified number of microseconds to number of ticks.                       |
+--------------------------------------------------------------------+-----------------------------------------------------------------------------------------+

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
~                                                                                                                                            



