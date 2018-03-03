CPU Time
========

The MyNewt ``cputime`` module provides high resolution time and timer
support.

Description
-----------

The ``cputime`` API provides high resolution time and timer support. The
module must be initialized, using the :c:func:`os_cputime_init()` function,
with the clock frequency to use. The module uses the ``hal_timer`` API,
defined in hal/hal_timer.h, to access the hardware timers. It uses the
hardware timer number specified by the ``OS_CPUTIME_TIMER_NUM`` system
configuration setting.

API
-----------------

.. doxygengroup:: OSCPUTime
    :content-only:
    :members:
