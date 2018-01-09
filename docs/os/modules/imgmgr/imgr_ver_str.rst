imgr\_ver\_str 
----------------

.. code-block:: console

       int
       imgr_ver_str(struct image_version *ver, char *dst)

Takes the version string from ``ver`` and formats that into a printable
string to ``dst``. Caller must make sure that ``dst`` contains enough
space to hold maximum length version string. The convenience
defininition for max length version string is named
``IMGMGR_MAX_VER_STR``.

Arguments
^^^^^^^^^

+-------------+-----------------------------------------------------------------------+
| Arguments   | Description                                                           |
+=============+=======================================================================+
| ver         | Image version number structure containing the value being formatted   |
+-------------+-----------------------------------------------------------------------+
| dst         | Pointer to C string where results will be stored                      |
+-------------+-----------------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Function returns the number of characters filled into the destination
string.

Notes
^^^^^

If build number is ``0`` in image version structure, it will be left out
of the string.

Example
^^^^^^^

.. code-block:: console

    static void
    imgr_ver_jsonstr(struct json_encoder *enc, char *key,
      struct image_version *ver)
    {
        char ver_str[IMGMGR_MAX_VER_STR];
        int ver_len;
        ...
        ver_len = imgr_ver_str(ver, ver_str)
        ...
    }
