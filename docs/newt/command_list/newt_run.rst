newt run 
---------

A single command to do four steps - build a target, create-image, load
image on a board, and start a debug session with the image on the board.

**Note**: If the version number is omitted:

-  The create-image step is skipped for a bootloader target.
-  You will be prompted to enter a version number for an application
   target.

Usage:
^^^^^^

.. code-block:: console

        newt run <target-name> [<version>][flags] 

Flags:
^^^^^^

.. code-block:: console

          --extrajtagcmd string   Extra commands to send to JTAG software
      -n, --noGDB                 Do not start GDB from the command line

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

Same as running ``build <target-name>``,
``create-image <target-name> <version>``, ``load <target-name>``, and
``debug <target-name>``.

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newt run       | Compiles and builds the  |
| blink\_rigado  | image for the ``app``    |
|                | and the ``bsp`` defined  |
|                | for target               |
|                | ``blink_rigado``, loads  |
|                | the image onto the       |
|                | board, and opens an      |
|                | active GNU gdb debugging |
|                | session to run the       |
|                | image.                   |
+----------------+--------------------------+--------------------+
| newt run       | Compiles and builds the  |
| ble\_rigado    | image for the ``app``    |
| 0.1.0.0        | and the ``bsp`` defined  |
|                | for target               |
|                | ``ble_rigado``, signs    |
|                | and creates the image    |
|                | with version number      |
|                | 0.1.0.0, loads the image |
|                | onto the board, and      |
|                | opens an active GNU gdb  |
|                | debugging session to run |
|                | the image. Note that if  |
|                | there is no bootloader   |
|                | available for a          |
|                | particular board/kit, a  |
|                | signed image creation    |
|                | step is not necessary.   |
+----------------+--------------------------+--------------------+
