newt size 
----------

Calculates the size of target components for a target.

Usage:
^^^^^^

.. code-block:: console

        newt size <target-name> [flags]

Flags:
^^^^^^

.. code-block:: console

        -F, --flash   Print FLASH statistics
        -R, --ram     Print RAM statistics

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

Displays the RAM and FLASH size of each component for the
``target-name`` target.

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newt size      | Inspects and lists the   |
| blink\_rigado  | RAM and Flash memory     |
|                | that each component      |
|                | (object files and        |
|                | libraries) for the       |
|                | ``blink_rigado`` target. |
+----------------+--------------------------+--------------------+

Example output for ``newt size blink_rigado``:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console


    $ newt size blink_rigado
      FLASH     RAM 
          9     223 *fill*
       1052       0 baselibc.a
        195    1116 blinky.a
        616     452 bmd300eval.a
         64       0 cmsis-core.a
        124       0 crt0.o
          8       0 crti.o
         16       0 crtn.o
        277     196 full.a
         20       8 hal.a
         96    1068 libg.a
       1452       0 libgcc.a
        332      28 nrf52xxx.a
       3143     677 os.a

    objsize
       text    data     bss     dec     hex filename
       7404    1172    2212   10788    2a24 /Users/<username>/dev/rigado/bin/blink_rigado/apps/blinky/blinky.elf
