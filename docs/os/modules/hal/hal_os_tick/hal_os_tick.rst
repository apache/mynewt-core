OS Tick 
=============

The hardware independent interface to set up interrupt timers or halt
CPU in terms of OS ticks.

Description
~~~~~~~~~~~

Set up the periodic timer to interrupt at a frequency of
'os\_ticks\_per\_sec' using the following function call where 'prio' is
the cpu-specific priority of the periodic timer interrupt.

.. code:: c

    void os_tick_init(uint32_t os_ticks_per_sec, int prio);

You can halt CPU for up to ``n`` ticks:

.. code:: c

    void os_tick_idle(os_time_t n);

The function implementations are in the mcu-specific directories such as
``hw/mcu/nordic/nrf51xxx/src/hal_os_tick.c``.

API
~~~~~~~~~~

.. doxygengroup:: HALOsTick
    :content-only:
    :members:


