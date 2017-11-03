How to Reduce Application Code Size
-----------------------------------

Gettng your application to fit in an image slot can be challenging,
particularly on flash constrained hardware such as the nRF51. Below are
some suggested system configuration settings that reduce the code size
of your Mynewt image.

+------------------------+---------------------------------------------+
| Setting                | Description                                 |
+========================+=============================================+
| LOG\_LEVEL: 255        | Disable all logging.                        |
+------------------------+---------------------------------------------+
| LOG\_CLI: 0            | Disable log shell commands.                 |
+------------------------+---------------------------------------------+
| STATS\_CLI: 0          | Disable stats shell commands.               |
+------------------------+---------------------------------------------+
| SHELL\_TASK: 0         | Disable the interactive shell.              |
+------------------------+---------------------------------------------+
| SHELL\_OS\_MODULE: 0   | Disable memory management shell commands.   |
+------------------------+---------------------------------------------+
| SHELL\_CMD\_HELP: 0    | Disable help for shell commands.            |
+------------------------+---------------------------------------------+

You can use the ``newt target set`` command to set the syscfg settings
in the ``syscfg.yml`` file for the target. See the `Newt Tool Command
Guide </newt/command_list/newt_target>`__ for the command syntax.

**Note:** The ``newt target set`` command deletes all the current syscfg
settings in the target ``syscfg.yml`` file and only sets the syscfg
settings specified in the command. If you are experimenting with
different settings to see how they affect the code size and do not want
to reenter all the setting values in the ``newt target set`` command,
you can use the ``newt target amend`` command. This command adds or
updates only the settings specified in the command and does not
overwrite other setting values. While you can also edit the target
``syscfg.yml`` file directly, we recommend that you use the
``newt target`` commands.
