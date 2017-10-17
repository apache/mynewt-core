 shell\_register 
-----------------

.. code:: c

    shell_register(const char *module_name, const struct shell_cmd *commands)

Registers a module named ``module_name`` and the commands that the
module supports. The caller must allocate and not free the memory for
the ``module_name`` and the array of ``shell_cmd`` structures for the
command. The shell keeps references to these structures for internal
use.

Each entry in the ``commands`` array specifies a shell command for the
module and must be initialized with the command name and the pointer to
the command handler. The help field is initialized with help information
if the command supports help.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| ``module_nam | Character      |
| e``          | string of the  |
|              | module name.   |
+--------------+----------------+
| ``commands`` | Array of       |
|              | ``shell_cmd``  |
|              | structures     |
|              | that specify   |
|              | the commands   |
|              | for the        |
|              | module. The    |
|              | ``sc_cmd``,    |
|              | ``sc_cmd_func` |
|              | `,             |
|              | and ``help``   |
|              | fields in the  |
|              | last entry     |
|              | must be set to |
|              | NULL to        |
|              | indicate the   |
|              | last entry in  |
|              | the array.     |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success.

Non-zero on failure. #### Notes The ``SHELL_MAX_MODULES`` syscfg setting
specifies the maximum number of modules the shell supports. This
function aborts if the number of registered modules exceeds this limit.
You can increase the value for this setting. #### Example This is an
example excerpt that shows how to declare and initialize the data
structures for a module and some shell commands for the. Variables for
the help structures are only declared and intialized if the
``SHELL_CMD_HELP`` syscfg setting is enabled. The ``sample_commands``
array of ``shell_cmd`` structures are declared and initialized. The
fields in the last entry are all set to NULL to indicate the last entry
in the array.

.. code:: c

    static int shell_sample_tasks_display_cmd(int argc, char **argv);
    static int shell_sample_mpool_display_cmd(int argc, char **argv);

    static const char *sample_module = "sample_module";

    /* 
     * Initialize param and command help information if SHELL_CMD_HELP 
     * is enabled.
     */

    #if MYNEWT_VAL(SHELL_CMD_HELP)
    static const struct shell_param sample_tasks_params[] = {
        {"", "task name"},
        {NULL, NULL}
    };

    static const struct shell_cmd_help sample_tasks_help = {
        .summary = "show tasks info",
        .usage = NULL,
        .params = sample_tasks_params,
    };

    static const struct shell_param sample_mpool_params[] = {
        {"", "mpool name"},
        {NULL, NULL}
    };

    static const struct shell_cmd_help sample_mpool_help = {
        .summary = "show system mpool",
        .usage = NULL,
        .params = sample_mpool_params,
    };

    #endif 

    /* 
     * Initialize an array of shell_cmd structures for the commands
     * in the os module.
     */
    static const struct shell_cmd sample_module_commands[] = {
        {
            .sc_cmd = "tasks",
            .sc_cmd_func = shell_sample_tasks_display_cmd,
    #if MYNEWT_VAL(SHELL_CMD_HELP)
            .help = &sample_tasks_help,
    #endif
        },
        {
            .sc_cmd = "sample_mpool",
            .sc_cmd_func = shell_sample_mpool_display_cmd,
    #if MYNEWT_VAL(SHELL_CMD_HELP)
            .help = &sample_mpool_help,
    #endif
        },
        { NULL, NULL, NULL },
    };


    void
    sample_module_init(void)
    {
        shell_register(sample_module, sample_module_commands);
        
    }
