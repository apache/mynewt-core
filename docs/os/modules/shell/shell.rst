Shell
=====

The shell runs above the console and provides two functionalities:

-  Processes console input. See the `Enabling the Console and Shell
   tutorial </os/tutorials/blinky_console.html>`__ for example of the
   shell.
-  Implements the `newtmgr <../../../newtmgr/overview.html>`__ line
   protocol over serial transport.

The shell uses the OS default event queue for shell events and runs in
the context of the main task. An application can, optionally, specify a
dedicated event queue for the shell to use.

The ``sys/shell`` package implements the shell. To use the shell you
must:

-  Include the ``sys/shell`` package.
-  Set the ``SHELL_TASK`` syscfg setting value to 1 to enable the shell.

**Note:** The functions for the shell API are only compiled and linked
with the application when the ``SHELL_TASK`` setting is enabled. When
you develop a package that supports shell commands, we recommend that
your pakcage define:

1. A syscfg setting that enables shell command processing for your
   package, with a restriction that when this setting is enabled, the
   ``SHELL_TASK`` setting must also be enabled.
2. A conditional dependency on the ``sys/shell`` package when the
   setting defined in 1 above is enabled.

Here are example definitions from the ``syscfg.yml`` and ``pkg.yml``
files for the ``sys/log/full`` package. It defines the ``LOG_CLI``
setting to enable the log command in the shell:

.. code-block:: console

    # sys/log/full syscfg.yml
     LOG_CLI:
            description: 'Expose "log" command in shell.'
            value: 0
            restrictions:
                - SHELL_TASK

    # sys/log/full pkg.yml
    pkg.deps.LOG_CLI:
        - sys/shell

Description
-----------

Processing Console Input Commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The shell's first job is to direct incoming commands to other
subsystems. It parses the incoming character string into tokens and uses
the first token to determine the subsystem command handler to call to
process the command. When the shell calls the command handler, it passes
the other tokens as arguments to the handler.

Registering Command Handlers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A package that implements a shell command must register a command
handler to process the command.

**New in release 1.1**: The shell supports the concept of modules and
allows a package to group shell commands under a name space. To run a
command in the shell, you enter the module name and the command name.
You can set a default module, using the ``select`` command, so that you
only need to enter the command name to run a command from the default
module. You can switch the module you designate as the default module.

There are two methods to register command handlers in Mynewt 1.1:

-  Method 1 (New in release 1.1): Define and register a set of commands
   for a module. This method allows grouping shell commands into
   namespaces. A package calls the ``shell_register()`` function to
   define a module and register the command handlers for the module.

   **Note:** The ``SHELL_MAX_MODULES`` syscfg setting specifies the
   maximum number of modules that can be registered. You can increase
   this value if your application and the packages it includes register
   more than the default value.

-  Method 2: Register a command handler without defining a module. A
   package calls the ``shell_cmd_register()`` function defined in Mynewt
   1.0 to register a command handler. When a shell command is registered
   using this method, the command is automatically added to the
   ``compat`` module. The ``compat`` module supports backward
   compatibility for all the shell commands that are registered using
   the ``shell_cmd_register()`` function.

   **Notes:**

   -  The ``SHELL_COMPAT`` syscfg setting must be set to 1 to enable
      backward compatibility support and the ``shell_cmd_register()``
      function. Since Mynewt packages use method 2 to register shell
      commands and Mynewt plans to continue this support in future
      releases, you must keep the default setting value of 1.

   -  The ``SHELL_MAX_COMPAT_COMMANDS`` syscfg setting specifies the
      maximum number of command handlers that can be registered using
      this method. You can increase this value if your application and
      the packages it includes register more than the default value.

 ####Enabling Help Information for Shell Commands

The shell supports command help. A package that supports command help
initializes the ``struct shell_cmd`` data structure with help text for
the command before it registers the command with the shell. The
``SHELL_CMD_HELP`` syscfg setting enables or disbles help support for
all shell commands. The feature is enabled by default.

**Note:** A package that implements help for a shell command should only
initialize the help data structures within the
``#if MYNEWT_VAL(SHELL_CMD_HELP)`` preprocessor directive.

Enabling the OS and Prompt Shell Modules
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The shell implements the ``os`` and ``prompt`` modules. These modules
support the shell commands to view OS resources.

The ``os`` module implements commands to list task and mempool usage
information and to view and change the time of day. The
``SHELL_OS_MODULE`` syscfg setting enables or disables the module. The
module is enabled by default.

The ``prompt`` module implements the ``ticks`` command that controls
whether to print the current os ticks in the prompt. The
``SHELL_PROMPT_MODULE`` syscfg setting enables or disables this module.
The module is disabled by default.

 #### Enabling Command Name Completion

The shell supports command name completion. The ``SHELL_COMPLETION``
syscfg setting enables or disables the feature. The feature is enabled
by default.

 ### Processing Newtmgr Line Protocol Over Serial Transport

The shell's second job is to handle packet framing, encoding, and
decoding of newtmgr protocol messages that are sent over the console.
The Newtmgr serial transport package
(``mgmt/newtmgr/transport/newtmgr_shell``) calls the
``shell_nlip_input_register()`` function to register a handler that the
shell calls when it receives newtmgr request messages.

The ``SHELL_NEWTMGR`` syscfg setting specifies whether newtmgr is
enabled over shell. The setting is enabled by default.

Data Structures
---------------

 The ``struct shell_cmd`` data structure represents a shell command and
is used to register a command.

.. code-block:: console

    struct shell_cmd {
        const char *sc_cmd;
        shell_cmd_func_t sc_cmd_func;
        const struct shell_cmd_help *help;
    };

+------------+----------------+
| Element    | Description    |
+============+================+
| ``sc_cmd`` | Character      |
|            | string of the  |
|            | command name.  |
+------------+----------------+
| ``sc_cmd_f | Pointer to the |
| unc_t``    | command        |
|            | handler that   |
|            | processes the  |
|            | command.       |
+------------+----------------+
| ``help``   | Pointer to the |
|            | shell\_cmd\_he |
|            | lp             |
|            | structure. If  |
|            | the pointer is |
|            | NULL, help     |
|            | information is |
|            | not provided.  |
+------------+----------------+

The ``sc_cmd_func_t`` is the command handler function type.

.. code:: c

    typedef int (*shell_cmd_func_t)(int argc, char *argv[]);

The ``argc`` parameter specifies the number of command line arguments
and the ``argv`` parameter is an array of character pointers to the
command arguments. The ``SHELL_CMD_ARGC_MAX`` syscfg setting specifies
the maximum number of command line arguments that any shell command can
have. This value must be increased if a shell command requires more than
``SHELL_CMD_ARGC_MAX`` number of command line arguments.

The ``struct shell_module`` data structure represents a shell module. It
is used to register a shell module and the shell commands for the
module.

.. code:: c

    struct shell_module {
        const char *name;
        const struct shell_cmd *commands;
    };

+------------+----------------+
| Element    | Description    |
+============+================+
| ``name``   | Character      |
|            | string of the  |
|            | module name.   |
+------------+----------------+
| ``commands | Array of       |
| ``         | ``shell_cmd``  |
|            | structures     |
|            | that specify   |
|            | the commands   |
|            | for the        |
|            | module. The    |
|            | ``sc_cmd``,    |
|            | ``sc_cmd_func` |
|            | `,             |
|            | and ``help``   |
|            | fields in the  |
|            | last entry     |
|            | must be set to |
|            | NULL to        |
|            | indicate the   |
|            | last entry in  |
|            | the array.     |
+------------+----------------+

**Note**: A command handler registered via the ``shell_cmd_register()``
function is automatically added to the ``compat`` module.

 The ``struct shell_param`` and ``struct shell_cmd_help`` data
structures hold help texts for a shell command.

.. code:: c

    struct shell_param {
        const char *param_name;
        const char *help;
    };`

+------------------+--------------------------------------------------------+
| Element          | Description                                            |
+==================+========================================================+
| ``param_name``   | Character string of the command parameter name.        |
+------------------+--------------------------------------------------------+
| ``help``         | Character string of the help text for the parameter.   |
+------------------+--------------------------------------------------------+

.. code:: c

    struct shell_cmd_help {
        const char *summary;
        const char *usage;
        const struct shell_param *params;
    };

+------------+----------------+
| Element    | Description    |
+============+================+
| ``summary` | Character      |
| `          | string of a    |
|            | short          |
|            | description of |
|            | the command.   |
+------------+----------------+
| ``usage``  | Character      |
|            | string of a    |
|            | usage          |
|            | description    |
|            | for the        |
|            | command.       |
+------------+----------------+
| ``params`` | Array of       |
|            | ``shell_param` |
|            | `              |
|            | structures     |
|            | that describe  |
|            | each parameter |
|            | for the        |
|            | command. The   |
|            | last           |
|            | ``struct shell |
|            | _param``       |
|            | in the array   |
|            | must have the  |
|            | ``param_name`` |
|            | and ``help``   |
|            | fields set to  |
|            | NULL to        |
|            | indicate the   |
|            | last entry in  |
|            | the array.     |
+------------+----------------+

 ##List of Functions

The functions available in this OS feature are:

+------------+----------------+
| Function   | Description    |
+============+================+
| `shell\_cm | Registers a    |
| d\_registe | handler for    |
| r <shell_c | incoming       |
| md_registe | console        |
| r.html>`__   | commands.      |
+------------+----------------+
| `shell\_ev | Specifies a    |
| q\_set <sh | dedicated      |
| ell_evq_se | event queue    |
| t.html>`__   | for shell      |
|            | events.        |
+------------+----------------+
| `shell\_nl | Registers a    |
| ip\_input\ | handler for    |
| _register  | incoming       |
| <shell_nli | newtmgr        |
| p_input_re | messages.      |
| gister.html> |                |
| `__        |                |
+------------+----------------+
| `shell\_nl | Queue outgoing |
| ip\_output | newtmgr        |
|  <shell_nl | message for    |
| ip_output. | transmission.  |
| md>`__     |                |
+------------+----------------+
| `shell\_re | Registers a    |
| gister <sh | shell module   |
| ell_regist | and the        |
| er.html>`__  | commands for   |
|            | the module.    |
+------------+----------------+
| `shell\_re | Registers a    |
| gister\_ap | command        |
| p\_cmd\_ha | handler as an  |
| ndler <she | application    |
| ll_registe | handler. The   |
| r_app_cmd_ | shell calls    |
| handler.md | this handler   |
| >`__       | when a command |
|            | does not have  |
|            | a handler      |
|            | registered.    |
+------------+----------------+
| `shell\_re | Registers a    |
| gister\_de | module with a  |
| fault\_mod | specified name |
| ule <shell | as the default |
| _register_ | module.        |
| default_mo |                |
| dule.html>`_ |                |
| _          |                |
+------------+----------------+
