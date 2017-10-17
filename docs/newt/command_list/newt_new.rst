newt new 
---------

Create a new project from a skeleton. Currently, the default skeleton is
the `blinky repository <https://github.com/apache/mynewt-blinky>`__.

Usage:
^^^^^^

.. code-block:: console

        newt new <project-name> [flags]

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
~~~~~~~~~~~

Creates a new project named ``project-name`` from the default skeleton
`blinky repository <https://github.com/apache/mynewt-blinky>`__.

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newt new       | Creates a new project    |
| test\_project  | named ``test_project``   |
|                | using the default        |
|                | skeleton from the        |
|                | ``apache/mynewt-blinky`` |
|                | repository.              |
+----------------+--------------------------+--------------------+
