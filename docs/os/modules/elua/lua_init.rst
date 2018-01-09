lua\_init 
-----------

.. code-block:: console

       int
       lua_init(void)

Registers 'lua' command with shell. This function should be called while
initializing the project, preferably after shell itself has been
initialized.

Arguments
^^^^^^^^^

N/A

Returned values
^^^^^^^^^^^^^^^

Returns

Notes
^^^^^

Calling this is meaningful only if you include the shell package in your
project.

Example
^^^^^^^

.. code-block:: console

    int main(int argc, char **argv)
    {
        ...
        shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE,
                             SHELL_MAX_INPUT_LEN);
        ...
        lua_init();
        ...
    }
