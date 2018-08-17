testutil
========

The testutil package is a test framework that provides facilities for
specifying test cases and recording test results.

You would use it to build regression tests for your library.

.. contents::
  :local:
  :depth: 2

Description
~~~~~~~~~~~

A package may optionally contain a set of test cases. Test cases are not
normally compiled and linked when a package is built; they are only
included when the "test" identity is specified. All of a package's test
code goes in its ``src/test`` directory. For example, the nffs package's
test code is located in the following directory:

.. code-block:: console

        * fs/nffs/src/test/

This directory contains the source and header files that implement the
nffs test code.

The test code has access to all the header files in the following
directories:

.. code-block:: console

        * src
        * src/arch/<target-arch>
        * include
        * src/test
        * src/test/arch/<target-arch>
        * include directories of all package dependencies

Package test code typically depends on the testutil package, described
later in this document.

Some test cases or test initialization code may be platform-specific. In
such cases, the platform-specific function definitions are placed in
arch subdirectories within the package test directory.

While building the test code (i.e., when the ``test`` identity is
specified), the newt tool defines the ``TEST`` macro. This macro is
defined during compilation of all C source files in all projects and
packages.

Tests are structured according to the following hierarchy:

.. code-block:: console

                    [test]
                   /      \
            [suite]        [suite]
           /       \      /       \
         [case] [case]  [case] [case]

I.e., a test consists of test suites, and a test suite consists of test
cases.

The test code uses testutil to define test suites and test cases.

Regression test can then be executed using 'newt target test' command,
or by including a call to your test suite from
``project/test/src/test.c``.

Example
~~~~~~~

:doc:`This Tutorial <../../../tutorials/other/unit_test>` shows how to create a
test suite for a Mynewt package.

Data structures
~~~~~~~~~~~~~~~

.. code-block:: console

    struct ts_config {
        int ts_print_results;
        int ts_system_assert;

        const char *ts_suite_name;

        /*
         * Called prior to the first test in the suite
         */
        tu_init_test_fn_t *ts_suite_init_cb;
        void *ts_suite_init_arg;

        /*
         * Called after the last test in the suite
         */
        tu_init_test_fn_t *ts_suite_complete_cb;
        void *ts_suite_complete_arg;

        /*
         * Called before every test in the suite
         */
        tu_pre_test_fn_t *ts_case_pre_test_cb;
        void *ts_case_pre_arg;

        /*
         * Called after every test in the suite
         */
        tu_post_test_fn_t *ts_case_post_test_cb;
        void *ts_case_post_arg;

        /*
         * Called after test returns success
         */
        tu_case_report_fn_t *ts_case_pass_cb;
        void *ts_case_pass_arg;

        /*
         * Called after test fails (typically thoough a failed test assert)
         */
        tu_case_report_fn_t *ts_case_fail_cb;
        void *ts_case_fail_arg;

        /*
         * restart after running the test suite - self-test only
         */
        tu_suite_restart_fn_t *ts_restart_cb;
        void *ts_restart_arg;
    };

The global ``ts_config`` struct contains all the testutil package's
settings.

API
~~~~

.. doxygengroup:: OSTestutil
    :members:
    :content-only:
