Build-Time Hooks
----------------

A package specifies custom commands in its ``pkg.yml`` file.  There are
three types of commands:

1. pre_build_cmds (run before the build)
2. pre_link_cmds (run after compilation, before linking)
3. post_link_cmds (run after linking)

Example
~~~~~~~

Example (apps/blinky/pkg.yml):

.. code-block:: yaml

    pkg.pre_build_cmds:
        scripts/pre_build1.sh: 100
        scripts/pre_build2.sh: 200

    pkg.pre_link_cmds:
        scripts/pre_link.sh: 500

    pkg.post_link_cmds:
        scripts/post_link.sh: 100


For each command, the string on the left specifies the command to run.
The number on the right indicates the command's relative ordering.  All
paths are relative to the project root.

When newt builds this example, it performs the following sequence:

- scripts/pre_build1.sh
- scripts/pre_build2.sh
- [compile]
- scripts/pre_link.sh
- [link]
- scripts/post_link.sh

If other packages specify custom commands, those commands would also be
executed during the above sequence.  For example, if another package
specifies a pre build command with an ordering of 150, that command
would run immediately after ``pre_build1.sh``.  In the case of a tie,
the commands are run in lexicographic order (by path).

All commands are run from the project's base directory.  In the above
example, the ``scripts`` directory is a sibling of ``targets``.

Custom Build Inputs
~~~~~~~~~~~~~~~~~~~

A custom pre-build or pre-link command can produce files that get fed
into the current build.

*Pre-build* commands can generate any of the following:

1. ``.c`` files for newt to compile.
2. ``.a`` files for newt to link.
3. ``.h`` files that any package can include.

*Pre-link* commands can only generate .a files.

``.c`` and ``.a`` files should be written to the
``$MYNEWT_USER_SRC_DIR`` environment variable (defined by newt), or any
subdirectory within.

``.h`` files should be written to ``$MYNEWT_USER_INCLUDE_DIR``.  The
directory structure used here is directly reflected by the includer.
E.g., if a script writes to ``$MYNEWT_USER_INCLUDE_DIR/foo/bar.h``,
then a source file can include this header with:

.. code-block:: cpp

    #include "foo/bar.h"

Details
~~~~~~~

Environment Variables
^^^^^^^^^^^^^^^^^^^^^

In addition to the usual environment variables defined for debug and
download scripts, newt defines the following env vars for custom
commands:

========================== ================================================================================= ===============================
  Environment variable      Description                                                                       Notes
-------------------------- --------------------------------------------------------------------------------- -------------------------------
  MYNEWT_APP_BIN_DIR        The directory where the current target's binary gets written.
  MYNEWT_PKG_BIN_ARCHIVE    The path to the current package's ``.a`` file.
  MYNEWT_PKG_BIN_DIR        The directory where the current package's ``.o`` and ``.a`` files get written.
  MYNEWT_PKG_NAME           The full name of the current package.
  MYNEWT_USER_INCLUDE_DIR   Path where globally-accessible headers get written.                               Pre-build only.
  MYNEWT_USER_SRC_DIR       Path where build inputs get written.                                              Pre-build and pre-link only.
  MYNEWT_USER_WORK_DIR      Shared temp directory; used for communication between commands.
========================== ================================================================================= ===============================

These environment variables are defined for each process that a custom
command runs in.  They are *not* defined in the newt process itself.
So, the following snippet will not produce the expected output:

BAD Example (apps/blinky/pkg.yml):

.. code-block:: yaml

    pkg.pre_cmds:
        'echo $MYNEWT_USER_SRC_DIR': 100

You can execute ``sh`` here instead if you need access to the
environment variables, but it is probably saner to just use a script.

Detect Changes in Custom Build Inputs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To avoid unnecessary rebuilds, newt detects if custom build inputs have
changed since the previous build.  If none of the inputs have changed,
then they do not get rebuilt.  If any of them have changed, they all
get rebuilt.

The ``$MYNEWT_USER_[...]`` directories are actually temp directories.
After the pre-build commands have run, newt compares the contents of
the temp directory with those of the actual user directory.  If any
differences are detected, newt replaces the user directory with the
temp directory, triggering a rebuild of its contents.  The same
procedure is used for pre-link commands.

Paths
^^^^^

Custom build inputs get written to the following directories:

- bin/targets/\<target\>/user/pre_build/src
- bin/targets/\<target\>/user/pre_build/include
- bin/targets/\<target\>/user/pre_link/src

Custom commands should not write to these directories.  They should use
the ``$MYNEWT_USER_[...]`` environment variables instead.
