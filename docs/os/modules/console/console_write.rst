 console\_write 
----------------

.. code-block:: console

       void
       console_write(char *str, int cnt)

Queues characters to console display over serial port.

Arguments
^^^^^^^^^

+-------------+--------------------------------------------------------+
| Arguments   | Description                                            |
+=============+========================================================+
| \*str       | pointer to the character or packet to be transmitted   |
+-------------+--------------------------------------------------------+
| cnt         | number of characters in *str*                          |
+-------------+--------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Example
^^^^^^^

Here is an example of the function being used in an echo command with a
newline at the end.

.. code-block:: console

    static int
    shell_echo_cmd(int argc, char **argv)
    {
        int i;

        for (i = 1; i < argc; i++) {
            console_write(argv[i], strlen(argv[i]));
            console_write(" ", sizeof(" ")-1);
        }
        console_write("\n", sizeof("\n")-1);

        return (0);
    }
