Validation and Error Messages
-----------------------------

| With multiple packages defining and overriding system configuration
  settings, it is easy to introduce conflicts and violations that are
  difficult to find. The ``newt build <target-name>`` command validates
  the setting definitions and value overrides for all the packages in
  the target to ensure a valid and consistent build. It aborts the build
  when it detects violations or ambiguities between packages.
| The following sections describe the error conditions that newt detects
  and the error messages that it outputs. For most errors, newt also
  outputs the ``Setting history`` with the order of package overrides to
  help you resolve the errors.

**Note:** The ``newt target config <target-name>`` command also detects
errors and outputs error messages at the top of the command output. The
command outputs the package setting definitions and values after it
outputs the error messages. It is easy to miss the error messages at the
top.

Value Override Violations
~~~~~~~~~~~~~~~~~~~~~~~~~

The newt tool uses package priorities to resolve override conflicts. It
uses the value override from the highest priority package when multiple
packages override the same setting. Newt checks for the following
override violations:

-  Ambiguity Violation - Two packages of the same priority override a
   setting with different values. And no higher priority package
   overrides the setting.
-  Priority Violation - A package overrides a setting defined by a
   package with higher or equal priority.

   **Note:** A package may override the default value for a setting that
   it defines. For example, a package defines a setting with a default
   value but needs to conditionally override the value based on another
   setting value.

 ####Example: Ambiguity Violation Error Message

The following example shows the error message that newt outputs for an
ambiguity violation:

.. code-block:: console


    Error: Syscfg ambiguities detected:
        Setting: LOG_NEWTMGR, Packages: [apps/slinky, apps/splitty]
    Setting history (newest -> oldest):
        LOG_NEWTMGR: [apps/splitty:0, apps/slinky:1, sys/log/full:0]

The above error occurs because the ``apps/slinky`` and ``apps/splitty``
packages in the split image target both override the same setting with
different values. The ``apps/slinky`` package sets the ``sys/log/full``
package ``LOG_NEWTMGR`` setting to 1, and the ``apps/splitty`` package
sets the setting to 0. The overrides are ambiguous because both are
``app`` packages and have the same priority. The following are excerpts
of the defintion and the two overrides from the ``syscfg.yml`` files
that cause the error:

.. code-block:: console


    #Package: sys/log/full
    syscfg.defs:
        LOG_NEWTMGR:
            description: 'Enables or disables newtmgr command tool logging'
            value: 0

    #Package: apps/slinky
    syscfg.vals:
        LOG_NEWTMGR: 1

    #Package: apps/splitty
    syscfg.vals:
        LOG_NEWTMGR: 0

Example: Priority Violation Error Message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following example shows the error message that newt outputs for a
priority violation where a package tries to change the setting that was
defined by another package at the same priority level:

.. code-block:: console


    Error: Priority violations detected (Packages can only override settings defined by packages of lower priority):
        Package: mgmt/newtmgr overriding setting: LOG_NEWTMGR defined by sys/log/full

    Setting history (newest -> oldest):
        LOG_NEWTMGR: [sys/log/full:0]

The above error occurs because the ``mgmt/newtmgr`` lib package
overrides the ``LOG_NEWTMGR`` setting that the ``sys/log/full`` lib
package defines. The following are excerpts of the definition and the
override from the ``syscfg.yml`` files that cause this error:

.. code-block:: console


    #Package: sys/log/full
    syscfg.defs:
         LOG_NEWTMGR:
            description: 'Enables or disables newtmgr command tool logging'
            value: 0

    #Package: mgmt/newtmgr
    syscfg.vals:
        LOG_NEWTMGR: 1

Flash Area Violations
~~~~~~~~~~~~~~~~~~~~~

For ``flash_owner`` type setting definitions, newt checks for the
following violations:

-  An undefined flash area is assigned to a setting.
-  A flash area is assigned to multiple settings.

Example: Undefined Flash Area Error Message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following example shows the error message that newt outputs for an
undefined flash area.

.. code-block:: console


    Building target targets/sim_slinky
    Error: Flash errors detected:
        Setting REBOOT_LOG_FLASH_AREA specifies unknown flash area: FLASH_AREA_NOEXIST

    Setting history (newest -> oldest):
        REBOOT_LOG_FLASH_AREA: [hw/bsp/native:FLASH_AREA_NOEXIST, sys/reboot:]

The above error occurs because the ``hw/bsp/native`` package assigns the
undefined ``FLASH_AREA_NOEXIST`` flash area to the ``sys/reboot``
package ``REBOOT_LOG_FLASH_AREA`` setting. The following are excerpts of
the definition and the override from the ``syscfg.yml`` files that cause
the error:

.. code-block:: console


    #Package: sys/reboot
    syscfg.defs:
        REBOOT_LOG_FLASH_AREA:
            description: 'Flash Area to use for reboot log.'
            type: flash_owner
            value:

    #Package: hw/bsp/native
    syscfg.vals:
        REBOOT_LOG_FLASH_AREA: FLASH_AREA_NOEXIST

Example: Multiple Flash Area Assignment Error Message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following example shows the error message that newt outputs when
multiple settings are assigned the same flash area:

.. code-block:: console


    Error: Flash errors detected:
        Multiple flash_owner settings specify the same flash area
              settings: REBOOT_LOG_FLASH_AREA, CONFIG_FCB_FLASH_AREA
            flash area: FLASH_AREA_NFFS

    Setting history (newest -> oldest):
        CONFIG_FCB_FLASH_AREA: [hw/bsp/native:FLASH_AREA_NFFS, sys/config:]
        REBOOT_LOG_FLASH_AREA: [apps/slinky:FLASH_AREA_NFFS, sys/reboot:]

The above error occurs because the ``hw/bsp/native`` package assigns the
``FLASH_AREA_NFFS`` flash area to the ``sys/config/`` package
``CONFIG_FCB_FLASH_AREA`` setting, and the ``apps/slinky`` package also
assigns ``FLASH_AREA_NFFS`` to the ``sys/reboot`` package
``REBOOT_LOG_FLASH_AREA`` setting. The following are excerpts of the two
definitions and the two overrides from the ``syscfg.yml`` files that
cause the error:

.. code-block:: console


    # Package: sys/config
    syscfg.defs.CONFIG_FCB:
        CONFIG_FCB_FLASH_AREA:
            description: 'The flash area for the Config Flash Circular Buffer'
            type: 'flash_owner'
            value:

    # Package: sys/reboot
    syscfg.defs:
        REBOOT_LOG_FLASH_AREA:
            description: 'The flash area for the reboot log'
            type: 'flash_owner' 
            value:

    #Package: hw/bsp/native
    syscfg.vals:
         CONFIG_FCB_FLASH_AREA: FLASH_AREA_NFFS

    #Package: apps/slinky
    syscfg.vals: 
        REBOOT_LOG_FLASH_AREA: FLASH_AREA_NFFS

 ###Restriction Violations For setting definitions with ``restrictions``
specified, newt checks for the following violations:

-  A setting with a ``$notnull`` restriction does not have a value.
-  For a setting with expression restrictions, some required setting
   values in the expressions evaluate to false.

Example: $notnull Restriction Violation Error Message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following example shows the error message that newt outputs when a
setting with ``$notnull`` restriction does not have a value:

.. code-block:: console


    Error: Syscfg restriction violations detected:
        NFFS_FLASH_AREA must not be null 

    Setting history (newest -> oldest):
        NFFS_FLASH_AREA: [fs/nffs:]

The above error occurs because the ``fs/nffs`` package defines the
``NFFS_FLASH_AREA`` setting with a ``$notnull`` restriction and no
packages override the setting. The following is an excerpt of the
definition in the ``syscfg.yml`` file that causes the error:

.. code-block:: console


    #Package: fs/nffs
    syscfg.defs:
        NFFS_FLASH_AREA:
            description: 'The flash area to use for the Newtron Flash File System'
            type: flash_owner
            value:
            restrictions:
                - $notnull

Example: Expression Restriction Violation Error Message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following example shows the error message that newt outputs for an
expression restriction violation:

.. code-block:: console


    Error: Syscfg restriction violations detected:
        CONFIG_FCB=1 requires CONFIG_FCB_FLASH_AREA be set, but CONFIG_FCB_FLASH_AREA=

    Setting history (newest -> oldest):
        CONFIG_FCB: [targets/sim_slinky:1, sys/config:0]
        CONFIG_FCB_FLASH_AREA: [sys/config:]

The above error occurs because the ``sys/config`` package defines the
``CONFIG_FCB`` setting with a restriction that when set, requires that
the ``CONFIG_FCB_FLASH_AREA`` setting must also be set. The following
are excerpts of the definition and the override from the ``syscfg.yml``
files that cause the error:

.. code-block:: console


    # Package:  sys/config
    syscfg.defs:
        CONFIG_FCB:
            description: 'Uses Config Flash Circular Buffer'
            value: 0
            restrictions:
                - '!CONFIG_NFFS'
                - 'CONFIG_FCB_FLASH_AREA'

    # Package: targets/sim_slinky
    syscfg.vals:
        CONFIG_FCB: 1

 ###Task Priority Violations

For ``task_priority`` type setting definitions, newt checks for the
following violations:

-  A task priority number is assigned to multiple settings.
-  The task priority number is greater than 239.

Example: Duplicate Task Priority Assignment Error Message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following example shows the error message that newt outputs when a
task priority number is assigned to multiple settings.

**Note:** The settings used in this example are not actual
``apps/slinky`` and ``sys/shell`` settings. These settings are created
for this example because currently only one Mynewt package defines a
``task_priority`` type setting.

.. code-block:: console


    Error: duplicate priority value: setting1=SHELL_TASK_PRIORITY setting2=SLINKY_TASK_PRIORITY pkg1=apps/slinky pkg2=sys/shell value=1

The above error occurs because the ``apps/slinky`` package defines a
``SLINKY_TASK_PRIORITY`` setting with a default task priority of 1 and
the ``sys/shell`` package also defines a ``SHELL_TASK_PRIORITY`` setting
with a default task priority of 1.

Example: Invalid Task Priority Error Message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following example shows the error message that newt outputs when a
setting is assigned an invalid task priority value:

.. code-block:: console


    Error: invalid priority value: value too great (> 239); setting=SLINKY_TASK_PRIORITY value=240 pkg=apps/slinky

The above error occurs because the ``apps/slinky`` package defines the
``SLINKY_TASK_PRIORITY`` setting with 240 for the default task priority
value.

**Note:** Newt does not output the ``Setting history`` with task
priority violation error messages.

 ###Duplicate System Configuration Setting Definition

A setting definition must be unique. Newt checks that only one package
in the target defines a setting. The following example shows the error
message that newt outputs when multiple packages define the
``LOG_NEWTMGR`` setting:

.. code-block:: console


    Error: setting LOG_NEWTMGR redefined

**Note:** Newt does not output the ``Setting history`` with duplicate
setting error messages. ###Override of Undefined System Configuration
Setting

The ``newt build`` command ignores overrides of undefined system
configuration settings. The command does not print a warning when you
run it with the default log level. If you override a setting and the
value is not assigned to the setting, you may have misspelled the
setting name or a package no longer defines the setting. You have two
options to troubleshoot this problem:

-  Run the ``newt target config show`` command to see the configuration
   setting definitions and overrides.
-  Run the ``newt build -ldebug`` command to build your target with
   DEBUG log level.

Note: The ``newt build -ldebug`` command generates lots of output and we
recommend that you use the ``newt target config show`` command option.
####Example: Ignoring Override of Undefined Setting Message

The following example shows that the ``apps/slinky`` application
overrides the ``LOG_NEWTMGR`` setting but omits the **T** as an example
of an error and overrides the misspelled **LOG\_NEWMGR** setting. Here
is an excerpt from its ``syscfg.yml`` file:

.. code-block:: console

    #package: apps/slinky
    syscfg.vals:
        # Enable the shell task.
        SHELL_TASK: 1
            ...

        # Enable newtmgr commands.
        STATS_NEWTMGR: 1
        LOG_NEWMGR: 1

 The ``newt target config show slinky_sim`` command outputs the
following WARNING message:

.. code-block:: console


    2017/02/18 17:19:12.119 [WARNING] Ignoring override of undefined settings:
    2017/02/18 17:19:12.119 [WARNING]     LOG_NEWMGR
    2017/02/18 17:19:12.119 [WARNING]     NFFS_FLASH_AREA
    2017/02/18 17:19:12.119 [WARNING] Setting history (newest -> oldest):
    2017/02/18 17:19:12.119 [WARNING]     LOG_NEWMGR: [apps/slinky:1]
    2017/02/18 17:19:12.119 [WARNING]     NFFS_FLASH_AREA: [hw/bsp/native:FLASH_AREA_NFFS]

The ``newt build -ldebug slinky_sim`` command outputs the following
DEBUG message:

.. code-block:: console


    2017/02/18 17:06:21.451 [DEBUG] Ignoring override of undefined settings:
    2017/02/18 17:06:21.451 [DEBUG]     LOG_NEWMGR
    2017/02/18 17:06:21.451 [DEBUG]     NFFS_FLASH_AREA
    2017/02/18 17:06:21.451 [DEBUG] Setting history (newest -> oldest):
    2017/02/18 17:06:21.451 [DEBUG]     LOG_NEWMGR: [apps/slinky:1]
    2017/02/18 17:06:21.451 [DEBUG]     NFFS_FLASH_AREA: [hw/bsp/native:FLASH_AREA_NFFS]

 #### BSP Package Overrides Undefined Configuration Settings

You might see a warning that indicates your application's BSP package is
overriding some undefined settings. As you can see from the previous
example, the WARNING message shows that the ``hw/bsp/native`` package is
overriding the undefined ``NFFS_FLASH_AREA`` setting. This is not an
error because of the way a BSP package defines and assigns its flash
areas to packages that use flash memory.

A BSP package defines, in its ``bsp.yml`` file, a flash area map of the
flash areas on the board. A package that uses flash memory must define a
flash area configuration setting name. The BSP package overrides the
package's flash area setting with one of the flash areas from its flash
area map. A BSP package overrides the flash area settings for all
packages that use flash memory because it does not know the packages
that an application uses. When an application does not include one of
these packages, the flash area setting for the package is undefined. You
will see a message that indicates the BSP package overrides this
undefined setting.

Here are excerpts from the ``hw/bsp/native`` package's ``bsp.yml`` and
``syscfg.yml`` files for the ``slinky_sim`` target. The BSP package
defines the flash area map in its ``bsp.yml`` file and overrides the
flash area settings for all packages in its ``syscfg.yml`` file. The
``slinky_sim`` target does not use the ``fs/nffs`` package which defines
the ``NFFS_FLASH_AREA`` setting. Newt warns that the ``hw/bsp/native``
packages overrides the undefined ``NFFS_FLASH_AREA`` setting.

.. code-block:: consoles


    # hw/bsp/native bsp.yml
    bsp.flash_map:
        areas:
            # System areas.
            FLASH_AREA_BOOTLOADER:
                device: 0
                offset: 0x00000000
                size: 16kB

                ...

            FLASH_AREA_IMAGE_SCRATCH:
                device: 0
                offset: 0x000e0000
                size: 128kB

            # User areas.
            FLASH_AREA_REBOOT_LOG:
                user_id: 0
                device: 0
                offset: 0x00004000
                size: 16kB
            FLASH_AREA_NFFS:
                user_id: 1
                device: 0
                offset: 0x00008000

    # hw/bsp/native syscfg.yml
    syscfg.vals:
        NFFS_FLASH_AREA: FLASH_AREA_NFFS
        CONFIG_FCB_FLASH_AREA: FLASH_AREA_NFFS
        REBOOT_LOG_FLASH_AREA: FLASH_AREA_REBOOT_LOG
