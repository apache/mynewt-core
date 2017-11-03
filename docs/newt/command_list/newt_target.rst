newt target 
------------

Commands to create, delete, configure and query targets.

Usage:
^^^^^^

.. code-block:: console

        newt target [command] [flags]

Available Commands:
^^^^^^^^^^^^^^^^^^^

.. code-block:: console

        amend       Add, change, or delete values for multi-value target variables
        config      View or populate a target's system configuration settings
        copy        Copy target
        create      Create a target
        delete      Delete target
        dep         View target's dependency graph
        revdep      View target's reverse-dependency graph
        set         Set target configuration variable
        show        View target configuration variables

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

The target command provides subcommands to create, build, delete, and
query targets. The subcommands take one or two ``target-name``
arguments.

+---------------+---------------+
| Sub-command   | Explanation   |
+===============+===============+
+---------------+---------------+

amend \| The amend command allows you to add, change, or delete values
for multi-value target variables that you have set with the
``newt target set`` command. The format of the amend command is: newt
target amend <target-name> <var-name=var-value> [var-name=var-value...]
Specify the ``-d`` flag to delete values.The following multi-value
variables can be amended: ``aflags``, ``cflags``, ``lflags``,
``syscfg``. The ``var-value`` format depends on the ``var-name`` as
follows:

.. raw:: html

   <ul>

.. raw:: html

   <li>

``aflags``, ``cflags``, ``lflags``: A string of flags, with each flag
separated by a space. These variables are saved in the target's
``pkg.yml`` file.

.. raw:: html

   </li>

.. raw:: html

   <li>

``syscfg``: The ``syscfg`` variable allows you to assign values to
configuration settings in your target's ``syscfg.yml`` file. The format
is
``syscfg=setting-name1=setting-value1[:setting-name2=setting-value2...]``,
where ``setting-name1`` is a configuration setting name and
``setting-value1`` is the value to assign to ``setting-name1``. If
``setting-value1`` is not specified, the setting is set to value ``1``.
You use a ``:`` to delimit each setting when you amend multiple
settings.To delete a system configuration setting, you only need to
specify the setting name. For example,
``syscfg=setting-name1:setting-name2`` deletes configuration settings
named ``setting-name1`` and ``setting-name2``.

.. raw:: html

   </li>

.. raw:: html

   </ul>

config \| The config command allows you to view or populate a target's
system configuration settings. A target's system configuration settings
include the settings of all the packages it includes. The settings for a
package are listed in the package's ``syscfg.yml`` file. The ``config``
command has two subcommands: ``show`` and ``init``. The config show
<target-name> command displays the system configuration setting
definitions and values for all the packages that the ``target-name``
target includes. The config init <target-name> command populates the
target's ``syscfg.yml`` file with the system configuration values for
all the packages that the ``target-name`` target includes. copy \| The
copy <src-target> <dst-target> command creates a new target named
``dst-target`` by cloning the ``src-target`` target. create \| The
create <target-name> command creates an empty target named
``target-name``. It creates the ``targets/target-name`` directory and
the skeleton ``pkg.yml`` and ``target.yml`` files in the directory.
delete \| The delete <target-name> command deletes the description for
the ``target-name`` target. It deletes the 'targets/target-name'
directory. It does not delete the 'bin/targets/target-name' directory
where the build artifacts are stored. If you want to delete the build
artifacts, run the ``newt clean <target-name>`` command **before**
deleting the target. dep \| The dep <target-name> command displays a
dependency tree for the packages that the ``target-name`` target
includes. It shows each package followed by the list of libraries or
packages that it depends on. revdep \| The revdep <target-name> command
displays the reverse dependency tree for the packages that the
``target-name`` target includes. It shows each package followed by the
list of libraries or packages that depend on it. set \| The set
<target-name> <var-name=var-value> [var-name=var-value...] command sets
variables (attributes) for the <target-name> target. The set command
overwrites your current variable values. The valid ``var-name`` values
are: ``app``, ``bsp``, ``loader``, ``build_profile``, ``cflags``,
``lflags``, ``aflags``, ``syscfg``. The ``var-value`` format depends on
the ``var-name`` as follows:

.. raw:: html

   <ul>

.. raw:: html

   <li>

``app``, ``bsp``, ``loader``: @<source-path>, where ``source-path`` is
the directory containing the application or bsp source. These variables
are stored in the target's target.yml file. For a simulated target, e.g.
for software testing purposes, set ``bsp`` to
``@apache-mynewt-core/hw/bsp/native``.

.. raw:: html

   </li>

.. raw:: html

   <li>

``build_profile``: ``optimized`` or ``debug``

.. raw:: html

   </li>

.. raw:: html

   <li>

``aflags``, ``cflags``, ``lflags``: A string of flags, with each flag
separated by a space. These variables are saved in the target's
``pkg.yml`` file.

.. raw:: html

   </li>

.. raw:: html

   <li>

``syscfg``: The ``syscfg`` variable allows you to assign values to
configuration settings in your target's ``syscfg.yml`` file. The format
is
``syscfg=setting-name1=setting-value1[:setting-name2=setting-value2...]``,
where ``setting-name1`` is a configuration setting name and
``setting-value1`` is the value to assign to ``setting-name1``. If
``setting-value1`` is not specified, the setting is set to value ``1``.
You use a ``:`` to delimit each setting when you set multiple settings.

.. raw:: html

   </li>

.. raw:: html

   </ul>

You can specify ``var-name=`` or ``var-name=""`` to unset a variable
value. **Warning**: For multi-value variables, the command overrides all
existing values. Use the ``newt target amend`` command to change or add
new values for a multi-value variable after you have set the variable
value. The multi-value variables are: ``aflags``, ``cflags``,
``lflags``, and ``syscfg``.To display all the existing values for a
target variable (attribute), you can run the
``newt vals <variable-name>`` command. For example, ``newt vals app``
displays the valid values available for the variable ``app`` for any
target. show \| The show [target-name] command shows the values of the
variables (attributes) for the ``target-name`` target. When
``target-name`` is not specified, the command shows the variables for
all the targets that are defined for your project.

Examples
^^^^^^^^

+-----------------+--------------------------+--------------------+
| Sub-command     | Usage                    | Explanation        |
+=================+==========================+====================+
| amend           | newt target amend myble  | Changes (or adds)  |
|                 | syscfg=CONFIG\_NEWTMGR=0 | the                |
|                 | cflags="-DTEST"          | ``CONFIG_NEWTMGR`` |
|                 |                          | variable to value  |
|                 |                          | 0 in the           |
|                 |                          | ``syscfg.yml``     |
|                 |                          | file and adds the  |
|                 |                          | -DTEST flag to     |
|                 |                          | ``pkg.cflags`` in  |
|                 |                          | the ``pkg.yml``    |
|                 |                          | file for the       |
|                 |                          | ``myble`` target.  |
|                 |                          | Other syscfg       |
|                 |                          | setting values and |
|                 |                          | cflags values are  |
|                 |                          | not changed.       |
+-----------------+--------------------------+--------------------+
| amend           | newt target amend myble  | Deletes the        |
|                 | -d                       | ``LOG_LEVEL`` and  |
|                 | syscfg=LOG\_LEVEL:CONFIG | ``CONFIG_NEWTMGR`` |
|                 | \_NEWTMGR                | settings from the  |
|                 | cflags="-DTEST"          | ``syscfg.yml``     |
|                 |                          | file and the       |
|                 |                          | -DTEST flag from   |
|                 |                          | ``pkg.cflags`` for |
|                 |                          | the ``myble``      |
|                 |                          | target. Other      |
|                 |                          | syscfg setting     |
|                 |                          | values and cflags  |
|                 |                          | values are not     |
|                 |                          | changed.           |
+-----------------+--------------------------+--------------------+
| config show     | newt target config show  | Shows the system   |
|                 | rb\_blinky               | configuration      |
|                 |                          | settings for all   |
|                 |                          | the packages that  |
|                 |                          | the ``rb_blinky``  |
|                 |                          | target includes.   |
+-----------------+--------------------------+--------------------+
| config init     | newt target config init  | Creates and        |
|                 | my\_blinky               | populates the      |
|                 |                          | ``my_blinky``      |
|                 |                          | target's           |
|                 |                          | ``syscfg.yml``     |
|                 |                          | file with the      |
|                 |                          | system             |
|                 |                          | configuration      |
|                 |                          | setting values     |
|                 |                          | from all the       |
|                 |                          | packages that the  |
|                 |                          | ``my_blinky``      |
|                 |                          | target includes.   |
+-----------------+--------------------------+--------------------+
| copy            | newt target copy         | Creates the        |
|                 | rb\_blinky rb\_bletiny   | ``rb_bletiny``     |
|                 |                          | target by cloning  |
|                 |                          | the ``rb_blinky``  |
|                 |                          | target.            |
+-----------------+--------------------------+--------------------+
| create          | newt target create       | Creates the        |
|                 | my\_new\_target          | ``my_newt_target`` |
|                 |                          | target. It creates |
|                 |                          | the                |
|                 |                          | ``targets/my_new_t |
|                 |                          | arget``            |
|                 |                          | directory and      |
|                 |                          | creates the        |
|                 |                          | skeleton           |
|                 |                          | ``pkg.yml`` and    |
|                 |                          | ``target.yml``     |
|                 |                          | files in the       |
|                 |                          | directory.         |
+-----------------+--------------------------+--------------------+
| delete          | newt target delete       | Deletes the        |
|                 | rb\_bletiny              | ``rb_bletiny``     |
|                 |                          | target. It deletes |
|                 |                          | the                |
|                 |                          | ``targets/rb_bleti |
|                 |                          | ny``               |
|                 |                          | directory.         |
+-----------------+--------------------------+--------------------+
| dep             | newt target dep myble    | Displays the       |
|                 |                          | dependency tree of |
|                 |                          | all the package    |
|                 |                          | dependencies for   |
|                 |                          | the ``myble``      |
|                 |                          | target. It lists   |
|                 |                          | each package       |
|                 |                          | followed by a list |
|                 |                          | of packages it     |
|                 |                          | depends on.        |
+-----------------+--------------------------+--------------------+
| revdep          | newt target revdep myble | Displays the       |
|                 |                          | reverse dependency |
|                 |                          | tree of all the    |
|                 |                          | package            |
|                 |                          | dependencies for   |
|                 |                          | the ``myble``      |
|                 |                          | target. It lists   |
|                 |                          | each package       |
|                 |                          | followed by a list |
|                 |                          | of packages that   |
|                 |                          | depend on it.      |
+-----------------+--------------------------+--------------------+
| set             | newt target set myble    | Use ``bletiny`` as |
|                 | app=@apache-mynewt-core/ | the application to |
|                 | apps/bletiny             | build for the      |
|                 |                          | ``myble`` target.  |
+-----------------+--------------------------+--------------------+
| set             | newt target set myble    | Set ``pkg.cflags`` |
|                 | cflags="-DNDEBUG         | variable with      |
|                 | -Werror"                 | ``-DNDEBUG -Werror |
|                 |                          | ``                 |
|                 |                          | in the ``myble``   |
|                 |                          | target's           |
|                 |                          | ``pkg.yml`` file.. |
+-----------------+--------------------------+--------------------+
| set             | newt target set myble    | Sets the           |
|                 | syscfg=LOG\_NEWTMGR=0:CO | ``syscfg.vals``    |
|                 | NFIG\_NEWTMGR            | variable in the    |
|                 |                          | ``myble`` target's |
|                 |                          | ``syscfg.yml``     |
|                 |                          | file with the      |
|                 |                          | setting values:    |
|                 |                          | LOG\_NEWTMGR: 0    |
|                 |                          | and                |
|                 |                          | CONFIG\_NEWTMGR:   |
|                 |                          | 1. CONFIG\_NEWTMGR |
|                 |                          | is set to 1        |
|                 |                          | because a value is |
|                 |                          | not specified.     |
+-----------------+--------------------------+--------------------+
| set             | newt target set myble    | Unsets the         |
|                 | cflags=                  | ``pkg.cflags``     |
|                 |                          | variable in the    |
|                 |                          | ``myble`` target's |
|                 |                          | ``pkg.yml`` file.  |
+-----------------+--------------------------+--------------------+
| show            | newt target show myble   | Shows all variable |
|                 |                          | settings for the   |
|                 |                          | ``myble`` target,  |
|                 |                          | i.e. the values    |
|                 |                          | that app, bsp,     |
|                 |                          | build\_profile,    |
|                 |                          | cflags, aflags,    |
|                 |                          | ldflags, syscfg    |
|                 |                          | variables are set  |
|                 |                          | to. Note that not  |
|                 |                          | all variables have |
|                 |                          | to be set for a    |
|                 |                          | target.            |
+-----------------+--------------------------+--------------------+
| show            | newt target show         | Shows all the      |
|                 |                          | variable settings  |
|                 |                          | for all the        |
|                 |                          | targets defined    |
|                 |                          | for the project.   |
+-----------------+--------------------------+--------------------+
