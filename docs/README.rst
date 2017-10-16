Mynewt Documentation
####################

This folder holds the documentation for the `Apache Mynewt`_ project. It is built using `Sphinx`_. The `Apache Mynewt`_ source code also contains inline comments in `Doxygen`_ format to document the APIs.

.. contents::

Writing Documentation
=======================

`Sphinx`_ use reStructuredText. http://www.sphinx-doc.org/en/1.5.1/rest.html.

Embedding `Doxygen`_ generated source documentation is through the `Breathe`_ bridge. http://breathe.readthedocs.io/en/latest/.

Linking to source uses `Sphinx`_'s C++ domain. http://www.sphinx-doc.org/en/1.5.1/domains.html#id2

Building the Documentation
==========================

Setup (MacOS)
---------------

Note: This build toolchain is known to work on MacOS 10.11.

Prerequisites: 
***************

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


Toolchain Install:
*******************

.. code-block:: bash

   $ git clone https://github.com/sphinx-doc/sphinx.git sphinx

   $ cd sphinx && sudo -E python setup.py install && cd ..

   $ git clone https://github.com/michaeljones/breathe.git breathe

   $ cd breathe && sudo -E python setup.py install && cd ..
   
   $ brew install doxygen
   
   $ sudo pip install recommonmark 

Build
-------

.. code-block:: bash

  $ cd docs
  $ make htmldocs

You can preview the documentation at ``_build/html/index.html``.

Live Reload
----------------

For the adventurous. You can preview changes to documentation with live reload as follows.

Setup:
*********

.. code-block:: bash

  $ brew install node
  $ sudo npm install -g grunt
  $ cd docs
  $ npm install
  
Run:
******

.. code-block:: bash

  $ grunt

A browser should open and as you make changes it should update after a few seconds.



.. _Apache Mynewt: https://mynewt.apache.org/
.. _Sphinx: http://www.sphinx-doc.org/
.. _Doxygen: http://www.doxygen.org/
.. _Homebrew: http://brew.sh/
.. _Pip: https://pip.readthedocs.io/en/stable/installing/
.. _Breathe: http://breathe.readthedocs.io/en/latest/
