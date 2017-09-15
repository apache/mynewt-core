 TEST\_SUITE 
-------------

.. code-block:: console

    TEST_SUITE(test_suite_name)

Declares a test suite function with the following type
``int test_suite_name(void)``. This can then be called from either
*project/test*, or from main routine for package specific regression
test.

Arguments
^^^^^^^^^

+---------------------+--------------------------------------------------+
| Arguments           | Description                                      |
+=====================+==================================================+
| test\_suite\_name   | Used as the function name for this test suite.   |
+---------------------+--------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Return value is 0 if the test suite passed; nonzero if it failed.
Generally, the return code is not used. It is expected that the
individual test cases will pass/fail with tests done using
``TEST_ASSERT()``.

Example
^^^^^^^

.. code-block:: console

    TEST_SUITE(os_sem_test_suite)
    {
        os_sem_test_basic();
        os_sem_test_case_1();
        os_sem_test_case_2();
        os_sem_test_case_3();
        os_sem_test_case_4();
    }
