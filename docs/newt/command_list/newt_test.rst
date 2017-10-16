newt test 
----------

Execute unit tests for one or more packages.

Usage:
^^^^^^

.. code-block:: console

        newt test <package-name> [package-names...]  | all [flags]

Flags:
^^^^^^

.. code-block:: console

       -e, --exclude string   Comma separated list of packages to exclude

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

Executes unit tests for one or more packages. You specify a list of
packages, separated by space, to test multiple packages in the same
command, or specify ``all`` to test all packages. When you use the
``all`` option, you may use the ``-e`` flag followed by a comma
separated list of packages to exclude from the test.

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newt test      | Tests the ``kernel/os``  |
| @apache-mynewt | package in the           |
| -core/kernel/o | ``apache-mynewt-core``   |
| s              | repository.              |
+----------------+--------------------------+--------------------+
| newt test      | Tests the ``kernel/os``  |
| kernel/os      | and ``encoding/json``    |
| encoding/json  | packages in the current  |
|                | repository.              |
+----------------+--------------------------+--------------------+
| newt test all  | Tests all packages.      |
+----------------+--------------------------+--------------------+
| newt test all  | Tests all packages       |
| -e             | except for the           |
| net/oic,encodi | ``net/oic`` and the      |
| ng/json        | ``encoding/json``        |
|                | packages.                |
+----------------+--------------------------+--------------------+
