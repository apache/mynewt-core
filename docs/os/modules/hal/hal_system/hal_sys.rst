System
===========

A hardware independent interface for starting and resetting the system.

Description
~~~~~~~~~~~

The API allows the user to detect whether a debugger is connected,
sissue a soft reset, and enumerate the reset causes. The functions are
implemented in the MCU specific directories e.g. ``hal_reset_cause.c``,
``hal_system.c``, and ``hal_system_start.c`` in
``/hw/mcu/nordic/nrf52xxx/src/`` directory for Nordic nRF52 series of
chips.

API
~~~~~~~~~~

.. doxygengroup:: HALSystem
    :content-only:
    :members:

