 TEST\_CASE\_DECL 
------------------

.. code-block:: console

    TEST_CASE_DECL(test_case_name)

Declares a test case function with the following type
``int test_case_name(void)``. This can then be called from regression
test's ``TEST_SUITE()`` function. This is only required if the test case
function exists in a different file than the test suite. This will allow
the test suite to find the test case

Arguments
^^^^^^^^^

+--------------------+-------------------------------------------------+
| Arguments          | Description                                     |
+====================+=================================================+
| test\_case\_name   | Used as the function name for this test case.   |
+--------------------+-------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Return value is 0 if the test case passed; nonzero if it failed.
Generally, the return code is not used. It is expected that the case
will pass/fail with tests done using ``TEST_ASSERT()``.

Example file ``test_cases.h``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

    TEST_CASE_DECL(test_case_1)
    TEST_CASE_DECL(test_case_2)
    TEST_CASE_DECL(test_case_3)
