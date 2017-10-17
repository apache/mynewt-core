newt upgrade 
-------------

Upgrade project dependencies.

Usage:
^^^^^^

.. code-block:: console

        newt upgrade [flags] 

Flags:
^^^^^^

.. code-block:: console

        -f, --force   Force upgrade of the repositories to latest state in project.yml

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

Upagrades your project and package dependencies. If you have changed the
project.yml description for the project, you need to run this command to
update all the package dependencies.
