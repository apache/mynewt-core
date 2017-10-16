 console\_printf
----------------

.. code-block:: console

        void
        console_printf(const char *fmt, ...)

Writes a formatted message instead of raw output to the console. It
first composes a C string containing text specified as arguments to the
function or containing the elements in the variable argument list passed
to it using snprintf or vsnprintf, respectively. It then uses function
``console_write`` to output the formatted data (messages) on the
console.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| fmt          | Pointer to C   |
|              | string that    |
|              | contains a     |
|              | format string  |
|              | that follows   |
|              | the same       |
|              | specifications |
|              | as format in   |
|              | printf. The    |
|              | string is      |
|              | printed to     |
|              | console.       |
+--------------+----------------+
| ...          | Depending on   |
|              | the format     |
|              | string, the    |
|              | function may   |
|              | expect either  |
|              | a sequence of  |
|              | additional     |
|              | arguments to   |
|              | be used to     |
|              | replace a      |
|              | format         |
|              | specifier in   |
|              | the format     |
|              | string or a    |
|              | variable       |
|              | arguments      |
|              | list. va\_list |
|              | is a special   |
|              | type defined   |
|              | in in          |
|              | stdarg.h.      |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

None

Notes
^^^^^

While ``console_printf``, with its well understood formatting options in
C, is more convenient and easy on the eyes than the raw output of
``console_write``, the associated code size is considerably larger.

Example
^^^^^^^

Example #1:

.. code-block:: console

    char adv_data_buf[32];

    void
    task()
    {
       char adv_data_buf[32];

       console_printf("%s", adv_data_buf);
    }

Example #2:

.. code-block:: console

    struct exception_frame {
        uint32_t r0;
        uint32_t r1;

    struct trap_frame {
        struct exception_frame *ef;
        uint32_t r2;
        uint32_t r3;
    };

    void
    task(struct trap_frame *tf)
    {
         console_printf(" r0:%8.8x  r1:%8.8x", tf->ef->r0, tf->ef->r1);
         console_printf(" r8:%8.8x  r9:%8.8x", tf->r2, tf->r3);
    }
