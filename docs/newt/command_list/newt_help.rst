newt help 
----------

Display the help text for the newt command line tool:

.. code-block:: console

    Newt allows you to create your own embedded application based on the Mynewt 
    operating system. Newt provides both build and package management in a single 
    tool, which allows you to compose an embedded application, and set of 
    projects, and then build the necessary artifacts from those projects. For more 
    information on the Mynewt operating system, please visit 
    https://mynewt.apache.org/. 

Usage:
^^^^^^

.. code-block:: console

        newt help [command]

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

Available Commands:
^^^^^^^^^^^^^^^^^^^

.. code-block:: console

        build        Build one or more targets
        clean        Delete build artifacts for one or more targets
        create-image Add image header to target binary
        debug        Open debugger session to target
        info         Show project info
        install      Install project dependencies
        load         Load built target to board
        mfg          Manufacturing flash image commands
        new          Create a new project
        pkg          Create and manage packages in the current workspace
        run          build/create-image/download/debug <target>
        size         Size of target components
        sync         Synchronize project dependencies
        target       Command for manipulating targets
        test         Executes unit tests for one or more packages
        upgrade      Upgrade project dependencies
        vals         Display valid values for the specified element type(s)
        version      Display the Newt version number

Examples
^^^^^^^^

+--------------------+----------------------------------------------------------+---------------+
| Sub-command        | Usage                                                    | Explanation   |
+====================+==========================================================+===============+
| newt help target   | Displays the help text for the newt ``target`` command   |
+--------------------+----------------------------------------------------------+---------------+
| newt help          | Displays the help text for newt tool                     |
+--------------------+----------------------------------------------------------+---------------+
