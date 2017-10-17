 TEST\_ASSERT
-------------

.. code-block:: console

    TEST_ASSERT(expression, fail_msg, ...)

.. code-block:: console

    TEST_ASSERT_FATAL(expression, fail_msg, ...)

Asserts that the specified condition is true. If the expression is true,
nothing gets reported. *fail\_msg* will be printed out if the expression
is false. The expression argument is mandatory; the rest are optional.
The fail\_msg argument is a printf format string which specifies how the
remaining arguments are parsed.

``TEST_ASSERT_FATAL()`` causes the current test case to be aborted, if
expression fails.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| expression   | Condition      |
|              | being tested.  |
|              | If it fails,   |
|              | test is        |
|              | considered a   |
|              | failure, and a |
|              | message is     |
|              | printed out.   |
+--------------+----------------+
| fail\_msg    | Pointer to C   |
|              | string that    |
|              | contains a     |
|              | format string  |
|              | that follows   |
|              | the same       |
|              | specifications |
|              | as format in   |
|              | printf.        |
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

    TEST_CASE(config_test_insert)
    {
        int rc;

        rc = conf_register(&config_test_handler);
        TEST_ASSERT(rc == 0);
    }

Example #2:

.. code-block:: console

    TEST_CASE(nffs_test_unlink)
    {
        int rc;

        ....
        
        rc = nffs_format(nffs_area_descs);
        TEST_ASSERT_FATAL(rc == 0);

        ....
    }

Example #3:

.. code-block:: console


    static int 
    cbmem_test_case_1_walk(struct cbmem *cbmem, struct cbmem_entry_hdr *hdr, 
            void *arg)
    {
        ....

        rc = cbmem_read(cbmem, hdr, &actual, 0, sizeof(actual));
        TEST_ASSERT_FATAL(rc == 1, "Couldn't read 1 byte from cbmem");
        TEST_ASSERT_FATAL(actual == expected, 
                "Actual doesn't equal expected (%d = %d)", actual, expected);

        ....
    }
