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

+--------------+--------------+
| **Function** | **Descriptio |
|              | n**          |
+==============+==============+
| `os\_cputime | Delays for a |
| \_delay\_nse | specified    |
| cs <os_cputi | number of    |
| me_delay_nse | nanoseconds. |
| cs.html>`__    |              |
+--------------+--------------+
| `os\_cputime | Delays for a |
| \_delay\_tic | specified    |
| ks <os_cputi | number of    |
| me_delay_tic | ticks.       |
| ks.html>`__    |              |
+--------------+--------------+
| `os\_cputime | Delays for a |
| \_delay\_use | specified    |
| cs <os_cputi | number of    |
| me_delay_use | microseconds |
| cs.html>`__    | .            |
+--------------+--------------+
| `os\_cputime | Gets the     |
| \_get32 <os_ | current      |
| cputime_get3 | value of the |
| 2.html>`__     | cpu time.    |
+--------------+--------------+
| `os\_cputime | Initializes  |
| \_init <os_c | the cputime  |
| putime_init. | module.      |
| md>`__       |              |
+--------------+--------------+
| `os\_cputime | Converts the |
| \_nsecs\_to\ | specified    |
| _ticks <os_c | number of    |
| putime_nsecs | nanoseconds  |
| _to_ticks.md | to number of |
| >`__         | ticks.       |
+--------------+--------------+
| `os\_cputime | Converts the |
| \_ticks\_to\ | specified    |
| _nsecs <os_c | number of    |
| putime_ticks | ticks to     |
| _to_nsecs.md | number of    |
| >`__         | nanoseconds. |
+--------------+--------------+
| `os\_cputime | Converts the |
| \_ticks\_to\ | specified    |
| _usecs <os_c | number of    |
| putime_ticks | ticks to     |
| _to_usecs.md | number of    |
| >`__         | microseconds |
|              | .            |
+--------------+--------------+
| `os\_cputime | Initializes  |
| \_timer\_ini | a timer.     |
| t <os_cputim |              |
| e_timer_init |              |
| .html>`__      |              |
+--------------+--------------+
| `os\_cputime | Sets a timer |
| \_timer\_rel | to expire in |
| ative <os_cp | the          |
| utime_timer_ | specified    |
| relative.html> | number of    |
| `__          | microseconds |
|              | from the     |
|              | current      |
|              | time.        |
+--------------+--------------+
| `os\_cputime | Sets a timer |
| \_timer\_sta | to expire at |
| rt <os_cputi | the          |
| me_timer_sta | specified    |
| rt.html>`__    | cputime.     |
+--------------+--------------+
| `os\_cputime | Stops a      |
| \_timer\_sto | timer from   |
| p <os_cputim | running.     |
| e_timer_stop |              |
| .html>`__      |              |
+--------------+--------------+
| `os\_cputime | Converts the |
| \_usecs\_to\ | specified    |
| _ticks <os_c | number of    |
| putime_usecs | microseconds |
| _to_ticks.md | to number of |
| >`__         | ticks.       |
+--------------+--------------+

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
