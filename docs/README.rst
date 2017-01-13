Mynewt Documentation
####################

This folder holds the documentation for the `Apache Mynewt`_ project. It is built using `Sphinx`_. The `Apache Mynewt`_ source code also contains inline comments in `Doxygen`_ format to document the APIs.

.. contents::

Building the Documentation
==========================

Setup (MacOS)
*************

Note: This build toolchain is known to work on MacOS 10.11.

Prerequisites: 
--------------

* `homebrew`_

.. code-block:: bash

  $ brew --version
  Homebrew 1.1.7

* python

.. code-block:: bash

  $ python --version
  Python 2.7.10

* `pip`_

.. code-block:: bash

  $ pip --version
  pip 9.0.1 from /Library/Python/2.7/site-packages (python 2.7)


Install
--------------

.. code-block:: bash

   $ git clone https://github.com/sphinx-doc/sphinx.git sphinx

   $ cd sphinx && sudo -E python setup.py install && cd ..

   $ git clone https://github.com/michaeljones/breathe.git breathe

   $ cd breathe && sudo -E python setup.py install && cd ..
   
   $ brew install doxygen
   ....
   $ sudo pip install recommonmark 
   ....



.. _Apache Mynewt: https://mynewt.apache.org/
.. _Sphinx: http://www.sphinx-doc.org/
.. _Doxygen: http://www.doxygen.org/
.. _Homebrew: http://brew.sh/
.. _Pip: https://pip.readthedocs.io/en/stable/installing/
