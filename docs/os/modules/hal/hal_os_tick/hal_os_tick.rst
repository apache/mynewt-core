hal\_os\_tick
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

Definition
~~~~~~~~~~

`hal\_os\_tick.h <https://github.com/apache/incubator-mynewt-core/blob/master/hw/hal/include/hal/hal_os_tick.h>`__

Examples
~~~~~~~~

An example of the API being used by the OS kernel for the Cortex M0
architecture to initialize and start the system clock timer can be seen
in
`kernel/os/src/arch/cortex\_m0/os\_arch\_arm.c <https://github.com/apache/incubator-mynewt-core/blob/master/kernel/os/src/arch/cortex_m0/os_arch_arm.c>`__.
