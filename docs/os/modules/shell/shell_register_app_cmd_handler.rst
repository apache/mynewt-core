 shell\_register\_app\_cmd\_handler 
------------------------------------

.. code:: c

    void shell_register_app_cmd_handler(struct shell_cmd *sc)

Registers a command handler as an application command handler. The shell
calls the application command handler, if one is set, when it receives a
command that does not have a handler registered. When you implement a
shell command for your application, you can register an application
command handler. You do not need to define a command name for the shell
to use to lookup an application command handler.

For example, if your application uses the ``shell_cmd_register()``
function to register a handler for the ``myapp_cmd`` shell command and
the handler supports two subcommands, ``subcmd1`` and \`\ ``subcmd2``,
then you would enter ``myapp_cmd subcmd1`` and ``myapp_cmd subcmd2`` in
the shell to run the commands. If you register the handler as an
application command handler, then you would enter ``subcmd1`` and
``subcmd2`` in the shell to run the commands. #### Arguments

+-------------+----------------------------------------------------------------------+
| Arguments   | Description                                                          |
+=============+======================================================================+
| ``sc``      | Pointer to the ``shell_cmd`` structure for the comman to register.   |
+-------------+----------------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

None

Example
^^^^^^^

.. code:: c


    static int myapp_cmd_handler(int argc, char **argv);

    static struct shell_cmd myapp_cmd = {
        .sc_cmd = "",
        .sc_cmd_func = myapp_cmd_handler
    };

    void
    myapp_shell_init(void)
    {
        shell_register_app_cmd_handler(&myapp_cmd);
        ....
    }
