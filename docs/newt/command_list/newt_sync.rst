newt sync 
----------

Synchronize and refresh the contents of the local copy of all the
repositories used in the project with the latest updates maintained in
the remote repositories.

Usage:
^^^^^^

.. code-block:: console

        newt sync [flags]

Flags:
^^^^^^

.. code-block:: console

        -f, --force             Force overwrite of existing remote repository

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

Synchronize project dependencies and repositories. Prior to 1.0.0
release, the command deletes and resynchronizes each repository. Post
1.0.0, it will abort the synchronization if there are any local changes
to any repository. Using the -f to force overwrite of existing
repository will stash and save the changes while pulling in all the
latest changes from the remote repository.
