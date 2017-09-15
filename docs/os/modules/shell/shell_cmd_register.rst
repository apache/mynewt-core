 shell\_cmd\_register 
----------------------

.. code-block:: console

    int shell_cmd_register(struct shell_cmd *sc)

Registers a handler for incoming shell commands. The function adds the
shell command to the ``compat`` module.

The caller must initialize the ``shell_cmd`` structure with the command
name and the pointer to the command handler. The caller must not free
the memory for the command name string because the shell keeps a
reference to the memory for internal use.

Arguments
^^^^^^^^^

+-------------+-----------------------------------------------------------------------+
| Arguments   | Description                                                           |
+=============+=======================================================================+
| ``sc``      | Pointer to the ``shell_cmd`` structure for the command to register.   |
+-------------+-----------------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success.

Non-zero on failure. #### Notes

The ``SHELL_MAX_COMPAT_COMMANDS`` syscfg setting specifies the maximum
number of shell commands that the ``compat`` module supports. This
function aborts if the number of handlers registered exceeds this limit.
You can increase the value for this setting.

 #### Example

.. code-block:: console

    static int fs_ls_cmd(int argc, char **argv);

    static struct shell_cmd fs_ls_struct = {
        .sc_cmd = "ls",
        .sc_cmd_func = fs_ls_cmd
    };

    void
    fs_cli_init(void)
    {
        shell_cmd_register(&fs_ls_struct);
        ....
    }
