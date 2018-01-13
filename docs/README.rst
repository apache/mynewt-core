Mynewt Documentation
####################

This folder holds the documentation for the core OS of the
`Apache Mynewt`_ project. It is  built using `Sphinx`_.
The source code also contains inline comments in `Doxygen`_
format to document the APIs.

The complete project documentation can be found at `mynewt documentation`_

.. contents::

Writing Documentation
=======================

`Sphinx`_ use reStructuredText. http://www.sphinx-doc.org/en/1.5.1/rest.html.

Embedding `Doxygen`_ generated source documentation is through the `Breathe`_
bridge. http://breathe.readthedocs.io/en/latest/.

Linking to source uses `Sphinx`_'s C++ domain. http://www.sphinx-doc.org/en/1.5.1/domains.html#id2

Previewing Changes
==========================

In order to preview any changes you make you must first install a Sphinx/Breathe/Doxygen toolchain as
described at `mynewt documentation`_. Then:

.. code-block:: bash

  $ cd docs
  $ make clean && make preview && (cd _build/html && python -m SimpleHTTPServer 8080)



.. _Apache Mynewt: https://mynewt.apache.org/
.. _mynewt documentation: https://github.com/apache/mynewt-documentation
.. _Sphinx: http://www.sphinx-doc.org/
.. _Doxygen: http://www.doxygen.org/
.. _Breathe: http://breathe.readthedocs.io/en/latest/
