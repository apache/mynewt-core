 TEST\_PASS 
------------

.. code-block:: console

    TEST_PASS(msg, ...)

Reports a success result for the current test. This function is not
normally needed, as all successful tests automatically write an empty
pass result at completion. It is only needed when the success result
report should contain text. The msg argument is a printf format string
which specifies how the remaining arguments are parsed. The result file
produced by this function contains the following text:

.. code-block:: console

            |<file>:<line-number>| manual pass
            <msg>

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| msg          | This is a      |
|              | printf format  |
|              | string which   |
|              | specifies how  |
|              | the remaining  |
|              | arguments are  |
|              | parsed         |
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

After this function is called, the remainder of the test case is not
executed.
