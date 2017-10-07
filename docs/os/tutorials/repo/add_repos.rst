Adding Repositories to your Project
===================================

.. toctree::
   :hidden:

   create_repo
   private_repo
   upgrade_repo


What is a Repository
~~~~~~~~~~~~~~~~~~~~

A repository is a version-ed Mynewt project, which is a collection of
Mynewt packages organized in a specific way for redistribution.

What differentiates a repository from a Mynewt project is the presence
of a ``repository.yml`` file describing the repository. This will be
described below. For a basic understanding of repositories you may read
the `Newt Tool Manual <../../../newt/newt_intro.html>`__ and `How to
create repos <create_repo.html>`__.

**Note:** For the remainder of this document we'll use the term repo as
shorthand for a Mynewt repository.

Repos are useful because they are an organized way for the community to
share Mynewt packages and projects. In fact, the Mynewt-core is
distributed as a repo.

Why does Mynewt need additional repos?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Repos add functionality not included in the Mynewt core. New repos might
be created for several reasons.

-  **Expertise**. Individuals or organizations may have expertise that
   they want to share in the form of repos. For example a chip vendor
   may create a repo to hold the Mynewt support for their chips.
-  **Non-Core component**. Some components, although very useful to
   Mynewt users are not core to all Mynewt users. These are likely
   candidates to be held in different repos.
-  **Software licensing**. Some software have licenses that make them
   incompatible with the ASF (Apache Software Foundation) license
   policies. These may be valuable components to some Mynewt users, but
   cannot be contained in the ``apache-Mynewt-core``.

What Repos are in my Project
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The list of repos used by your project are contained within the
``project.yml`` file. An example can be seen by creating a new project:

.. code-block:: console

    $ mkdir ~/dev
    $ cd ~/dev
    $ newt new myproj
    $ cd myproj

View the ``project.yml`` section and you will see a line describing the
repos:

.. code-block:: console

    project.repositories:
        - apache-Mynewt-core

By default, this newly created project uses a single repo called
``apache-Mynewt-core``.

If you wish to add additional repos, you would add additional lines to
the ``project.repositories`` variable like this.

.. code:: hl_lines="3"

    project.repositories:
        - apache-Mynewt-core
        - another_repo_named_x

Repo Descriptors
~~~~~~~~~~~~~~~~

In addition to the repo name, the ``project.yml`` file must also contain
a repo descriptor for each repository you include that gives ``newt``
information on obtaining the repo.

In the same ``myproj`` above you will see the following repo descriptor.

.. code-block:: console

    repository.apache-Mynewt-core:
        type: github
        vers: 1-latest
        user: apache
        repo: mynewt-core

A repo descriptor starts with ``repository.<name>.``. In this example,
the descriptor specifies the information for the ``apache-Mynewt-core``.

The fields within the descriptor have the following definitions:

-  **type** -- The type of code storage the repo uses. The current
   version of ``newt`` only supports github. Future versions may support
   generic git or other code storage mechanisms.

-  **vers** -- The version of the repo to use for your project. A source
   code repository contains many versions of the source. This field is
   used to specify the one to use for this project. See the section on
   versions below for a detailed description of the format of this
   field.

-  **user** -- The username for the repo. On github, this is the name
   after ``github.com`` in the repo path. Consider the repository
   ``https://github.com/apache/mynewt-core``. It has username
   ``apache``.

-  **repo** -- The name of the repo. On github, this is the name after
   the username described above. Consider the repository
   ``https://github.com/apache/mynewt-core``. It has path
   ``mynewt-core``. This is a path to the source control and should not
   be confused with the name of the repo that you used in the
   ``repository.<name>`` declaration above. That name is contained
   elsewhere within the repo. See Below.

Adding Existing Repos to my Project
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To add a new repo to your project, you have to complete two steps.

-  Edit the ``project.yml`` file and add a new repo descriptor. The
   previous section includes information on the field required in your
   repo descriptor.

-  Edit the ``project/yml`` file and add a new line to the
   ``project.repositories`` variable with the name of the repo you are
   adding.

An example of a ``project.yml`` file with two repositories is shown
below:

.. code-block:: console

    project.name: "my_project"

    project.repositories:
        - apache-Mynewt-core
        - Mynewt_arduino_zero
        
    # Use github's distribution mechanism for core ASF libraries.
    # This provides mirroring automatically for us.
    #
    repository.apache-Mynewt-core:
        type: github
        vers: 1-latest
        user: apache
        repo: mynewt-core
        
    # a special repo to hold hardware specific stuff for arduino zero
    repository.Mynewt_arduino_zero:
        type: github
        vers: 1-latest
        user: runtimeco
        repo: Mynewt_arduino_zero

What Version of the Repo to use
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Mynewt repos are version-ed artifacts. They are stored in source control
systems like github. The repo descriptor in your ``project.yml`` file
must specify the version of the repo you will accept into your project.

For now, we are at the beginnings of Mynewt. For testing and evaluation
please use ``1-latest`` in the ``vers`` field in your repo descriptor.

::

        vers:1-latest

See `Create a Repo <create_repo>`__ for a description of the versioning
system and all the possible ways to specify a version to use.

Identifying a Repo
~~~~~~~~~~~~~~~~~~

A repo contains Mynewt packages organized in a specific way and stored
in one of the supported code storage methods described above. In other
words, it is a Mynewt project with an additional file ``repository.yml``
which describes the repo for use by ``newt`` (and humans browsing them).
It contains a mapping of version numbers to the actual github branches
containing the source code.

Note that the ``repository.yml`` file lives only in the master branch of
the git repository. ``Newt`` will always fetch this file from the master
branch and then use that to determine the actual branch required
depending on the version specified in your ``project.yml`` file. Special
care should be taken to ensure that this file exists only in the master
branch.

Here is the ``repository.yml`` file from the apache-Mynewt-core:

.. code-block:: console

    repo.name: apache-mynewt-core
    repo.versions:
        "0.0.0": "master"
        "0.0.1": "master"
        "0.7.9": "mynewt_0_8_0_b2_tag"
        "0.8.0": "mynewt_0_8_0_tag"
        "0.9.0": "mynewt_0_9_0_tag"
        "0.9.9": "mynewt_1_0_0_b1_tag"
        "0.9.99": "mynewt_1_0_0_b2_tag"
        "0.9.999": "mynewt_1_0_0_rc1_tag"
        "1.0.0": "mynewt_1_0_0_tag"

        "0-latest": "1.0.0"    # 1.0.0
        "0-dev": "0.0.0"       # master

        "0.8-latest": "0.8.0"
        "0.9-latest": "0.9.0"
        "1.0-latest": "1.0.0"  # 1.0.0

It contains the following:

-  **repo.name** The external name that is used to include the library
   in your ``project.yml`` file. This is the name you in include in the
   ``project.repositories`` variable when adding this repository to your
   project.
-  **repo.versions** A description of what versions to give the user
   depending on the settings in their ``project.yml`` file.

Repo Version
~~~~~~~~~~~~

The repo version number resolves to an actual git branch depending on
the mapping specified in ``repository.yml`` for that repo. The version
field argument in your ``project.yml`` file supports multiple formats
for flexibility:

.. code-block:: console

    <major_num>.<minor_num>.<revision_num>

or

.. code-block:: console

    <major_num>.<minor_num>-<stability string>

or

.. code-block:: console

    <major_num>-<stability string>

The stability string can be one of 3 pre-defined stability values.

1. stable -- A stable release version of the repository
2. dev -- A development version from the repository
3. latest -- The latest from the repository

In your ``project.yml`` file you can specify different combinations of
the version number and stability value. For example:

-  ``1-latest`` -- The latest version with major number 1
-  ``1.2-stable`` -- The latest stable version with major and minor
   number 1.2
-  ``1.2-dev`` -- The development version from 1.2
-  ``1.1.1`` -- a specific version 1.1.1

You cannot specify a stability string with a fully numbered version,
e.g.

.. code-block:: console

    1.2.8-stable

Repo Versions Available
~~~~~~~~~~~~~~~~~~~~~~~

A ``repository.yml`` file contains information to match a version
request into a git branch to fetch for your project.

It's up to the repository maintainer to map these to branches of the
repository. For example, let's say in a fictitious repository the
following are defined.

.. code-block:: console

    repo.versions:
        "0.8.0": "xxx_branch_0_8_0"
        "1.0.0": "xxx_branch_1_0_0"
        "1.0.2": "xxx_branch_1_0_2"
        "1.1.1": "xxx_branch_1_1_0"
        "1.1.2": "xxx_branch_1_1_2"
        "1.2.0": "xxx_branch_1_2_0"
        "1.2.1": "xxx_branch_1_2_1"
        "1.2-dev": "1.2.1"
        "1-dev": "1.2-dev"
        "1.2-stable": "1.2.0"
        "0-latest": "0.8.0"
        "1-latest": "1-dev"
        ....

When the ``project.yml`` file asks for ``1.2-stable`` it is resolved to
version ``1.2.0`` (perhaps ``1.2.1`` is not stable yet), which in turn
resolves to a specific branch ``xxx_branch_1_2_0``. This is the branch
that ``newt`` fetches into your project.

**Note:** Make sure a repo version exists in the ``repository.yml`` file
of a repo you wish to add. Otherwise Newt will not be able to resolve
the version and will fail to fetch the repo into your project.

How to find out what Repos are available for Mynewt components
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Currently, there is no ``newt`` command to locate/search Mynewt package
repositories. However, since the ``newt`` tool supports only github,
searching github by keyword is a satisfactory option until a search tool
is created.

When searching github, recall that a Mynewt repository must have a
``repository.yml`` file in its root directory. If you don't see that
file, it's not a Mynewt repository and can't be included in your project
via the newt tool.

Once you find a repository, the github URL and ``repository.yml`` file
should give you all the information to add it to your ``project.yml``
file.


