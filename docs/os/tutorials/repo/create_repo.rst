Create a Repo out of a Project
------------------------------

In order to create a repository out of a project, all you need to do is
create a ``repository.yml`` file, and check it into the master branch of
your project.

**NOTE:** Currently only github source control service is supported by
our package management system, but support for plain git will be added
soon.

The repository.yml defines all versions of the repository and the
corresponding source control tags that these versions correspond to. As
an example, if the repository.yml file has the following content, it
means there is one version of the apache-mynewt-core operating system
available, which is ``0.0.0`` (implying we haven't released yet!). Such
a version number corresponds to the "develop" branch in this repository.
``0-latest`` would also resolved to this same ``0.0.0`` version. The
next section explains the versioning system a bit more.

::

    $ more repository.yml
    repo.name: apache-mynewt-core
    repo.versions:
         "0.0.0": "develop"
         "0-latest": "0.0.0"

Where should the repository.yml file be?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``repository.yml`` file lives only in the master branch of the git
repository. ``Newt`` will always fetch this file from the master branch
and then use that to resolve the actual branch required depending on the
version specified in the project. **Special care should be taken to
ensure that this file exists only in the master branch.**

Here is the ``repository.yml`` file from a certain snapshot of
apache-Mynewt-core:

::

    repo.name: apache-mynewt-core
    repo.versions:
        "0.7.9": "Mynewt_0_8_0_b2_tag"
        "0-latest": "0.7.9"
        "0.8-latest": "0.7.9"

It contains the following:

-  **repo.name** The external name that is used to include the library
   in your ``project.yml`` file. This is the name you include in the
   ``project.repositories`` variable when adding this repository to your
   project.
-  **repo.versions** A description of what versions to give the user
   depending on the settings in their ``project.yml`` file. See below
   for a thorough description on versioning. Its a flexible mapping
   between version numbers and git branches.

Repo Version Specification
~~~~~~~~~~~~~~~~~~~~~~~~~~

The version field argument for a repo has the following format:

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

You **cannot** specify a stability string with a fully numbered version,
e.g.

.. code-block:: console

    1.2.8-stable

Repo Version Resolution
~~~~~~~~~~~~~~~~~~~~~~~

A ``repository.yml`` file contains information to match this version
request into a git branch to fetch for your project.

It's up to you as the repository maintainer to map these to actual
github branches of the repository. For example, let's say in a
fictitious repository the following are defined.

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

When the ``project.yml`` file asks for ``1.2-stable`` it will be
resolved to version ``1.2.0`` which in turn will resolve to a specific
branch ``xxx_branch_1_2_0``. This is the branch that ``newt`` will fetch
into the project with that ``project.yml`` file.

Dependencies on other repos
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Repositories can also have dependencies on other repositories. These
dependencies should be listed out on a per-tag basis. So, for example,
if apache-mynewt-core were to depend on sterlys-little-repo, you might
have the following directives in the repository.yml:

::

    develop.repositories:
        sterlys-little-repo:
            type: github
            vers: 0.8-latest
            user: sterlinghughes
            repo: sterlys-little-repo

This would tell Newt that for anything that resolves to the develop
branch, this repository requires the sterlys-little-repo repository.

Dependencies are resolved circularly by the newt tool, and every
dependent repository is placed as a sibling in the repos directory.
Currently, if two repositories have the same name, they will conflict
and bad things will happen.

When a repository is installed to the repos/ directory, the current
version of that repository is written to the "project.state" file. The
project state file contains the currently installed version of any given
repository. This way, the current set of repositories can be recreated
from the project.state file reliably, whereas the project.yml file can
have higher level directives (i.e. include 0.8-stable.)

Resolving dependencies
~~~~~~~~~~~~~~~~~~~~~~

At the moment, all dependencies must match, otherwise newt will provide
an error. As an example, if you have a set of dependencies such that:

::

    apache-mynewt-core depends on sterlys-little-repo 0.6-stable
    apache-mynewt-core depends on sterlys-big-repo 0.5.1
    sterlys-big-repo-0.5.1 depends on sterlys-little-repo 0.6.2

where 0.6-stable is 0.6.3, the newt tool will try and resolve the
dependency to sterlys-little-repo. It will notice that there are two
conflicting versions of the same repository, and not perform
installation.

In the future Newt will be smarter about loading in all dependencies,
and then looking to satisfy those dependencies to the best match of all
potential options.
