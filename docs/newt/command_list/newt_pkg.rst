newt pkg 
---------

Commands for creating and manipulating packages.

Usage:
^^^^^^

.. code-block:: console

        newt pkg [command] [flags] 

Flags:
^^^^^^

.. code-block:: console

     -t, --type string   Type of package to create: app, bsp, lib, sdk, unittest. (default "lib")

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

The pkg command provides subcommands to create and manage packages. The
subcommands take one or two ``package-name`` arguments.

+---------------------------+---------------------------+
| Sub-command               | Explanation               |
+===========================+===========================+
| copy                      | The copy <src-pkg>        |
|                           | <dst-pkg> command creates |
|                           | the new ``dst-pkg``       |
|                           | package by cloning the    |
|                           | ``src-pkg`` package.      |
+---------------------------+---------------------------+
| move                      | The move <old-pkg>        |
|                           | <new-pkg> command moves   |
|                           | the ``old-pkg`` package   |
|                           | to the ``new-pkg``        |
|                           | package.                  |
+---------------------------+---------------------------+
| new                       | The new <new-pkg> command |
|                           | creates a new package     |
|                           | named ``new-pkg``, from a |
|                           | template, in the current  |
|                           | directory. You can create |
|                           | a package of type         |
|                           | ``app``, ``bsp``,         |
|                           | ``lib``, ``sdk``, or      |
|                           | ``unittest``. The default |
|                           | package type is ``lib``.  |
|                           | You use the -t flag to    |
|                           | specify a different       |
|                           | package type.             |
+---------------------------+---------------------------+
| remove                    | The remove <my-pkg>       |
|                           | command deletes the       |
|                           | ``my-pkg`` package.       |
+---------------------------+---------------------------+

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| copy           | newt pkg copy            | Copies the         |
|                | apps/bletiny             | ``apps/bletiny``   |
|                | apps/new\_bletiny        | package to the     |
|                |                          | ``apps/new_bletiny |
|                |                          | ``.                |
+----------------+--------------------------+--------------------+
| move           | newt pkg move            | Moves the          |
|                | apps/slinky              | ``apps/slinky``    |
|                | apps/new\_slinky         | package to the     |
|                |                          | ``apps/new_slinky` |
|                |                          | `                  |
|                |                          | package.           |
+----------------+--------------------------+--------------------+
| new            | newt pkg new             | Creates a package  |
|                | apps/new\_slinky         | named              |
|                |                          | ``apps/new_slinky` |
|                |                          | `                  |
|                |                          | of type ``pkg`` in |
|                |                          | the current        |
|                |                          | directory.         |
+----------------+--------------------------+--------------------+
| new            | newt pkg new             | Creates a package  |
|                | hw/bsp/myboard -t bsp    | named              |
|                |                          | ``hw/bsp/myboard`` |
|                |                          | of type ``bsp`` in |
|                |                          | the current        |
|                |                          | directory.         |
+----------------+--------------------------+--------------------+
| remove         | newt pkg remove          | Removes the        |
|                | hw/bsp/myboard           | ``hw/bsp/myboard`` |
|                |                          | package.           |
+----------------+--------------------------+--------------------+
