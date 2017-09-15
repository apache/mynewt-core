newt install 
-------------

Install project dependencies.

Usage:
^^^^^^

.. code-block:: console

        newt install [flags]

Flags:
^^^^^^

.. code-block:: console

        -f, --force  Force install of the repositories in project, regardless of what exists in repos directory

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

This command downloads the description for all the repositories
specified in the ``project.yml`` file for the current project, and
installs the correct versions of all the packages specified by the
project dependencies.

You must run this command from within the current project directory.
(Remember to ``cd`` into this project directory after you use
``newt new`` to create this project before you run ``newt install``.)
