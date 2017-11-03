newt info 
----------

Show information about the current project.

Usage:
^^^^^^

.. code-block:: console

        newt info [flags]

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

Displays the repositories in the current project (the local as well as
all the external repositories fetched). It also displays the packages in
the local repository.
