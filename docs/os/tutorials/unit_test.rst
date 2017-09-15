Write a Test Suite for a Package
================================

This document guides the reader through creating a test suite for a
Mynewt package.

Introduction
------------

Writing a test suite involves using the
```test/testutil`` <../modules/testutil/testutil.html>`__ package. The
testutil library provides the functionality needed to define test suites
and test cases.

Choose Your Package Under Test
------------------------------

Choose the package you want to write a test suite for. In this tutorial,
we will use the ``time/datetime`` in the apache-mynewt-core repo.
Throughout this tutorial, we will be inside the apache-mynewt-core repo
directory, unlike most tutorials which operate from the top-level
project directory.

Create A Test Package
---------------------

Typically, a library has only one test package. The convention is name
the test package by appending ``/test`` to the host library name. For
example, the test package for ``encoding/json`` is
``encoding/json/test``. The directory structure of the json package is
shown below:

.. code:: c

    encoding/json
    ├── include
    │   └── json
    │       └── json.h
    ├── pkg.yml
    ├── src
    │   ├── json_decode.c
    │   └── json_encode.c
    └── test
        ├── pkg.yml
        └── src
            ├── test_json.c
            ├── test_json.h
            ├── test_json_utils.c
            └── testcases
                ├── json_simple_decode.c
                └── json_simple_encode.c

The top-level ``test`` directory contains the json test package. To
create a test package for the datetime package, we need to create a
similar package called ``time/datetime/test``.

::

    $ newt pkg new time/datetime/test -t unittest
    Download package template for package type pkg.
    Package successfuly installed into /home/me/mynewt-core/time/datetime/test.

We now have a test package inside ``time/datetime``:

::

    time/datetime
    ├── include
    │   └── datetime
    │       └── datetime.h
    ├── pkg.yml
    ├── src
    │   └── datetime.c
    └── test
        ├── README.md
        ├── pkg.yml
        ├── src
        │   └── main.c
        └── syscfg.yml

There is one modification we need to make to the new package before we
can start writing unit test code. A test package needs access to the
code it will be testing, so we need to add a dependency on
``@apache-mynewt-core/time/datetime`` to our ``pkg.yml`` file:

.. code:: hl_lines="10"

    pkg.name: "time/datetime/test"
    pkg.type: unittest
    pkg.description: "Description of your package"
    pkg.author: "You <you@you.org>"
    pkg.homepage: "http://your-url.org/"
    pkg.keywords:

    pkg.deps:
        - '@apache-mynewt-core/test/testutil'
        - '@apache-mynewt-core/time/datetime'

    pkg.deps.SELFTEST:
        - '@apache-mynewt-core/sys/console/stub'

While we have the ``pkg.yml`` file open, let's take a look at what newt
filled in automatically:

-  ``pkg.type: unittest`` designates this as a test package. A *test
   package* is special in that it can be built and executed using the
   ``newt test`` command.
-  A test package always depends on
   ``@apache-mynewt-core/test/testutil``. The testutil library provides
   the tools necessary for verifying package behavior,
-  The ``SELFTEST`` suffix indicates that a setting should only be
   applied when the ``newt test`` command is used.

Regarding the conditional dependency on ``sys/console/stub``, the
datetime package requires some form of console to function. In a regular
application, the console dependency would be supplied by a higher order
package. Because ``newt test`` runs the test package without an
application present, the test package needs to supply all unresolved
dependencies itself when run in self-test mode.

Create Your Test Suite Code
---------------------------

We will be adding a *test suite* to the ``main.c`` file. The test suite
will be empty for now. We also need to invoke the test suite from
``main()``.

Our ``main.c`` file now looks like this:

.. code:: c

    #include "sysinit/sysinit.h"
    #include "testutil/testutil.h"

    TEST_SUITE(test_datetime_suite) {
        /* Empty for now; add test cases later. */
    }

    #if MYNEWT_VAL(SELFTEST)
    int
    main(int argc, char **argv)
    {
        /* Initialize all packages. */
        sysinit();

        test_datetime_suite();

        /* Indicate whether all test cases passed. */
        return tu_any_failed;
    }
    #endif

Try It Out
----------

We now have a working test suite with no tests. Let's make sure we get a
passing result when we run ``newt test``:

::

    $ newt test time/datetime
    <build output>
    Executing test: /home/me/mynewt-core/bin/targets/unittest/time_datetime_test/app/time/datetime/test/time_datetime_test.elf
    Passed tests: [time/datetime/test]
    All tests passed

Create a Test
-------------

To create a test within your test suite, there are two things to do.

1. Implement the test case function using the ``testutil`` macros.
2. Call the test case function from within the test suite.

For this tutorial we will create a test case to verify the
``datetime_parse()`` function. The ``datetime_parse()`` function is
declared as follows:

.. code:: c

    /**
     * Parses an RFC 3339 datetime string.  Some examples of valid datetime strings
     * are:
     * 2016-03-02T22:44:00                  UTC time (implicit)
     * 2016-03-02T22:44:00Z                 UTC time (explicit)
     * 2016-03-02T22:44:00-08:00            PST timezone
     * 2016-03-02T22:44:00.1                fractional seconds
     * 2016-03-02T22:44:00.101+05:30        fractional seconds with timezone
     *
     * On success, the two output parameters are filled in (tv and tz).
     *
     * @return                      0 on success;
     *                              nonzero on parse error.
     */
    int
    datetime_parse(const char *input, struct os_timeval *tv, struct os_timezone *tz)

Our test case should make sure this function rejects invalid input, and
that it parses valid input correctly. The updated ``main.c`` file looks
like this:

.. code:: c

    #include "sysinit/sysinit.h"
    #include "testutil/testutil.h"
    #include "os/os_time.h"
    #include "datetime/datetime.h"

    TEST_SUITE(test_datetime_suite)
    {
        test_datetime_parse_simple();
    }

    TEST_CASE(test_datetime_parse_simple)
    {
        struct os_timezone tz;
        struct os_timeval tv;
        int rc;

        /*** Valid input. */

        /* No timezone; UTC implied. */
        rc = datetime_parse("2017-06-28T22:37:59", &tv, &tz);
        TEST_ASSERT_FATAL(rc == 0);
        TEST_ASSERT(tv.tv_sec == 1498689479);
        TEST_ASSERT(tv.tv_usec == 0);
        TEST_ASSERT(tz.tz_minuteswest == 0);
        TEST_ASSERT(tz.tz_dsttime == 0);

        /* PDT timezone. */
        rc = datetime_parse("2013-12-05T02:43:07-07:00", &tv, &tz);
        TEST_ASSERT_FATAL(rc == 0);
        TEST_ASSERT(tv.tv_sec == 1386236587);
        TEST_ASSERT(tv.tv_usec == 0);
        TEST_ASSERT(tz.tz_minuteswest == 420);
        TEST_ASSERT(tz.tz_dsttime == 0);

        /*** Invalid input. */

        /* Nonsense. */
        rc = datetime_parse("abc", &tv, &tz);
        TEST_ASSERT(rc != 0);

        /* Date-only. */
        rc = datetime_parse("2017-01-02", &tv, &tz);
        TEST_ASSERT(rc != 0);

        /* Zero month. */
        rc = datetime_parse("2017-00-28T22:37:59", &tv, &tz);
        TEST_ASSERT(rc != 0);

        /* 13 month. */
        rc = datetime_parse("2017-13-28T22:37:59", &tv, &tz);
        TEST_ASSERT(rc != 0);
    }

    #if MYNEWT_VAL(SELFTEST)
    int
    main(int argc, char **argv)
    {
        /* Initialize all packages. */
        sysinit();

        test_datetime_suite();

        /* Indicate whether all test cases passed. */
        return tu_any_failed;
    }
    #endif

Take a few minutes to review the above code. Then keep reading for some
specifics.

Asserting
~~~~~~~~~

The ``test/testutil`` package provides two tools for verifying the
correctness of a package:

-  ``TEST_ASSERT``
-  ``TEST_ASSERT_FATAL``

Both of these macros check if the supplied condition is true. They
differ in how they behave when the condition is not true. On failure,
``TEST_ASSERT`` reports the error and proceeds with the remainder of the
test case. ``TEST_ASSERT_FATAL``, on the other hand, aborts the test
case on failure.

The general rule is to only use ``TEST_ASSERT_FATAL`` when subsequent
assertions depend on the condition being checked. For example, when
``datetime_parse()`` is expected to succeed, the return code is checked
with ``TEST_ASSERT_FATAL``. If ``datetime_parse()`` unexpectedly failed,
the contents of the ``tv`` and ``tz`` objects would be indeterminate, so
it is desirable to abort the test instead of checking them and reporting
spurious failures.

Scaling Up
~~~~~~~~~~

The above example is small and self contained, so it is reasonable to
put everything in a single C file. A typical package will need a lot
more test code, and it helps to follow some conventions to maintain
organization. Let's take a look at a more realistic example. Here is the
directory structure of the ``fs/nffs/test`` package:

::

    fs/nffs/test
    ├── pkg.yml
    └── src
        ├── nffs_test.c
        ├── nffs_test.h
        ├── nffs_test_debug.c
        ├── nffs_test_priv.h
        ├── nffs_test_system_01.c
        ├── nffs_test_utils.c
        ├── nffs_test_utils.h
        └── testcases
            ├── append_test.c
            ├── cache_large_file_test.c
            ├── corrupt_block_test.c
            ├── corrupt_scratch_test.c
            ├── gc_on_oom_test.c
            ├── gc_test.c
            ├── incomplete_block_test.c
            ├── large_system_test.c
            ├── large_unlink_test.c
            ├── large_write_test.c
            ├── long_filename_test.c
            ├── lost_found_test.c
            ├── many_children_test.c
            ├── mkdir_test.c
            ├── open_test.c
            ├── overwrite_many_test.c
            ├── overwrite_one_test.c
            ├── overwrite_three_test.c
            ├── overwrite_two_test.c
            ├── read_test.c
            ├── readdir_test.c
            ├── rename_test.c
            ├── split_file_test.c
            ├── truncate_test.c
            ├── unlink_test.c
            └── wear_level_test.c

The ``fs/nffs/test`` package follows these conventions:

1. A maximum of one test case per C file.
2. Each test case file goes in the ``testcases`` subdirectory.
3. Test suites and utility functions go directly in the ``src``
   directory.

Test packages contributed to the Mynewt project should follow these
conventions.

Congratulations
---------------

Now you can begin the work of validating your packages.
