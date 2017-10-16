newt vals 
----------

Display valid values for the specified element type(s).

Usage:
^^^^^^

.. code-block:: console

      newt vals <element-type> [element-types...] [flags]

Global Flags:
^^^^^^^^^^^^^

.. code-block:: console

        -h, --help              Help for newt commands
        -j, --jobs int          Number of concurrent build jobs (default 8)
        -l, --loglevel string   Log level (default "WARN")
        -o, --outfile string    Filename to tee output to
        -q, --quiet             Be quiet; only display error output
        -s, --silent            Be silent; don't output anything
        -v, --verbose           Enable verbose output when executing commands

Description
^^^^^^^^^^^

Displays valid values for the specified element type(s). You must set
valid values for one or more elements when you define a package or a
target. Valid element types are:

-  api
-  app
-  bsp
-  build\_profile
-  compiler
-  lib
-  sdk
-  target

Examples
^^^^^^^^

+----------------+-----------------------+--------------------+
| Sub-command    | Usage                 | Explanation        |
+================+=======================+====================+
| newt vals api  | Shows the possible    |
|                | values for APIs a     |
|                | package may specify   |
|                | as required. For      |
|                | example, the          |
|                | ``pkg.yml`` for       |
|                | ``adc`` specifies     |
|                | that it requires the  |
|                | api named             |
|                | ``ADC_HW_IMPL``, one  |
|                | of the values listed  |
|                | by the command.       |
+----------------+-----------------------+--------------------+

Example output for ``newt vals bsp``:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This lists all possible values that may be assigned to a target's bsp
attribute.

.. code-block:: console

    $ newt vals bsp
    bsp names:
        @apache-mynewt-core/hw/bsp/arduino_primo_nrf52
        @apache-mynewt-core/hw/bsp/bmd300eval
        @apache-mynewt-core/hw/bsp/ci40
        @apache-mynewt-core/hw/bsp/frdm-k64f
        @apache-mynewt-core/hw/bsp/native
        @apache-mynewt-core/hw/bsp/nrf51-arduino_101
        @apache-mynewt-core/hw/bsp/nrf51-blenano
        @apache-mynewt-core/hw/bsp/nrf51dk
        @apache-mynewt-core/hw/bsp/nrf51dk-16kbram
        @apache-mynewt-core/hw/bsp/nrf52dk
        @apache-mynewt-core/hw/bsp/nucleo-f401re
        @apache-mynewt-core/hw/bsp/olimex_stm32-e407_devboard
        @apache-mynewt-core/hw/bsp/rb-nano2
        @apache-mynewt-core/hw/bsp/stm32f4discovery
    $ newt target set sample_target bsp=@apache-mynewt-core/hw/bsp/rb-nano2

Obviously, this output will grow as more board support packages are
added for new boards and MCUs.
