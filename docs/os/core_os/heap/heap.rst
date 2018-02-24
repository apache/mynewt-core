Heap
====

API for doing dynamic memory allocation.

Description
------------

This provides malloc()/free() functionality with locking. The shared
resource heap needs to be protected from concurrent access when OS has
been started. :c:func:`os_malloc()` function grabs a mutex before calling
``malloc()``.


API
----

.. doxygengroup:: OSMalloc
    :content-only:
    :members:
