 console\_is\_init 
-------------------

.. code-block:: console

       int console_is_init(void)

Returns whether console has been initialized or not.

Arguments
^^^^^^^^^

None

Returned values
^^^^^^^^^^^^^^^

1 if console has been initialized.

0 if console has not been initialized.

Example
^^^^^^^

.. code-block:: console

    static int
    log_console_append(struct log *log, void *buf, int len)
    {
        ....

        if (!console_is_init()) {
            return (0);
        }

        /* print log entry to console */
        ....
    }
