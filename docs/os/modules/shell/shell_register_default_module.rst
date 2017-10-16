 shell\_register\_default\_module
---------------------------------

.. code:: c

    void shell_register_default_module(const char *name)

Sets the module named ``name`` as the default module. You can enter the
commands for the default module without entering the module name in the
shell.

Arguments
^^^^^^^^^

+-------------+--------------------------------------------+
| Arguments   | Description                                |
+=============+============================================+
| ``name``    | Name of the module to set as the default   |
+-------------+--------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

None

Example
^^^^^^^

.. code:: c

    static int sample_cmd_handler(int argc, char **argv); 

    static const char * module_name = "sample_module";
    static const struct shell_cmd sample_cmds[] = {
        {
            .sc_cmd = "mycmd",
            .sc_cmd_func = sample_cmd_handler,
        },
        {NULL, NULL, NULL},
    };

    int main (void)
    {


        /* Register the module and the commands for the module */
        shell_register(module_name, sample_cmds);

        /* Set this module as the default module */
        shell_register_default_module(module_name);

    }
