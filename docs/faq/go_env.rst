Contributing to Newt or Newtmgr Tools
-------------------------------------

Newt and Newtmgr are written in Go (golang). This guide shows you how to
install Go and setup your environment to update and build the tools if
you want to:

-  Build the tools with latest updates from the master branch on Linux
   or Windows platforms.

   **Note:** For Mac OS, you can use the
   ``brew install mynewt-newt -HEAD`` and the
   ``brew install mynewt-newtmgr --HEAD`` commands.

-  Contribute to newt or newtmgr features or fix bugs.

This guide shows you how to perform the following:

1. Install Mac OS X, Linux, Windows. (Tasks that are specific to each
   platform are called out.)
2. Setup the Go environment.
3. Download the source, build, and install the newt or newtmgr tools.
4. Update and rebuild the tools.

**Note:** You will also need to read and follow the instructions from
the `FAQ </faq/answers/>`__ to set up your git repos to submit changes.

Step 1: Installing Go
~~~~~~~~~~~~~~~~~~~~~

The latest master branch of newt and newtmgr requires GO version 1.7.6
or higher. You can skip this step and proceed to Step 2 if you already
have Go version 1.7.6 or higher installed.

 #### Installing Go on Mac OS X

If you do not have Homebrew installed, run the following command. You
will be prompted for your sudo password.

.. code-block:: console

    $ ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

You can also extract (or ``git clone``) Homebrew and install it to
/usr/local.

 Use brew to install Go:

.. code-block:: console

    $ brew install go
    ==> 
    ...
    ... 
    ==> Summary
    🍺  /usr/local/Cellar/go/1.8.3: 7,035 files, 282.0MB

You can also download the Go package directly from
(https://golang.org/dl/) and install it in /usr/local/bin instead of
brewing it.

 #### Installing Go on Linux and Windows You can download Go from
https://golang.org/dl/.

 ###Step 2: Setting Up Your Go Environment

This section describes the Go environment and how to setup a Go
workspace. If you already have a Go workspace for your other Go
projects, you can skip this step and proceed to Step 3.

Go provides an environment to compile Go code, construct Go packages,
and import Go code. You will use Go commands to import the newt or
newtmgr package repository into your local Go environment. The Go
language environment dictates a specific directory structure, or
workspace in Go parlance. It must contain three sibling directories with
the names **src**, **pkg** and **bin**:

-  src contains Go source files organized into packages (one package per
   directory)
-  pkg contains package objects
-  bin contains the Go application executables that Go builds and
   installs.

The **GOPATH** environment variable specifies the location of your
workspace. To setup this workspace environment, create a **dev**
directory and then a **go** directory under it. Set the GOPATH
environment variable to this directory where you will clone the newt and
newtmgr repositories.

.. code-block:: console

    $ cd $HOME
    $ mkdir -p dev/go  
    $ cd dev/go
    $ export GOPATH=`pwd`

 Add the following export statements to your ~/.bash\_profile file and
source the file:

.. code-block:: console

    export GOPATH=$HOME/dev/go
    export PATH=$GOPATH/bin:$PATH

Step 3: Downloading the Source and Installing the Tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Newt and newtmgr are individual Go packages and have their own git
repositories. You can download the source and install one or both tools.

We use the ``go get`` command to download the source, build, and install
the binary in the **$GOPATH/bin** directory.

 #### Downloading and Installing the Newt Tool

The newt Go package is **mynewt.apache.org/newt/newt** and is stored in
the `Apache Mynewt newt tool repository mirrored on
github <https://github.com/apache/mynewt-newt>`__.

Download the newt package source and install the tool:

.. code-block:: console

    $cd $GOPATH
    $go get mynewt.apache.org/newt/newt
    $cd $GOPATH/src/mynewt.apache.org/newt
    $ls 
    DISCLAIMER      RELEASE_NOTES.md    util
    INSTALLING.md       build.sh        viper
    LICENSE         newt            yaml
    NOTICE          newtmgr
    README.md       newtvm
    $git status
    On branch master
    Your branch is up-to-date with 'origin/master'.
    nothing to commit, working directory clean

 **Note:** The source code under the **newtmgr** directory is no longer
used or updated. The current **newtmgr** source has its own Git
repository.

 Check that the newt binary is installed and you are using the one from
\*\* $GOPATH/bin\*\*:

.. code-block:: console

    $ls $GOPATH/bin/newt
    ~/dev/go/bin/newt
    $which newt
    ~/dev/go/bin/newt
    $newt version
    Apache Newt version: 1.1.0-dev

 #### Downloading and Installing the Newtmgr Tool

The newtmgr Go package is **mynewt.apache.org/newtmgr/newtmgr**. It is
stored in the `Apache Mynewt newtmgr tool repository mirrored on
github <https://github.com/apache/mynewt-newtmgr>`__.

Download the newtmgr package and install the tool:

.. code-block:: console

    $cd $GOPATH
    $go get mynewt.apache.org/newtmgr/newtmgr
    $cd $GOPATH/src/mynewt.apache.org/newtmgr
    $ls
    LICENSE     NOTICE      README.md   newtmgr     nmxact
    $git status
    On branch master
    Your branch is up-to-date with 'origin/master'.
    nothing to commit, working directory clean

 Check that the newtmgr binary is installed and you are using the one
from **$GOPATH/bin**:

.. code-block:: console

    $ls $GOPATH/bin/newtmgr
    ~/dev/go/bin/newtmgr
    $which newtmgr
    ~/dev/go/bin/newtmgr

 ### Step 4: Updating and Rebuilding the Tools: This section shows you
how to rebuild the newt and newtmgr tools with the latest updates from
the master branch or after you have made changes in your branch.

Here is the general procedure to rebuild either the newt or newtmgr
tool. The only difference is the directory where you will be executing
the commands from. You will need to repeat the procedure to rebuild both
tools.

1. Change to the directory where the local Git repository for the tool
   is installed.
2. Pull the latest changes from the master branch. If you made changes
   you will need to rebase with **origin master** (See
   `FAQ </faq/answers/>`__).
3. Build and install the tool.

 Change to the directory where the source for the tool is installed.

For the **newt** tool:

.. code-block:: console

    $cd $GOPATH/src/mynewt.apache.org/newt/newt

For the **newtmgr** tool:

.. code-block:: console

    $cd $GOPATH/src/mynewt.apache.org/newtmgr/newtmgr

 After you change to the specific tool directory, get the latest updates
from the master branch. If you made changes and need to rebase with the
origin, add the ``--rebase origin master`` arguments to the ``git pull``
command:

.. code-block:: console

    $git pull 

 Build and install the tool. The updated binary will be installed in the
**$GOPATH/bin** directory:

.. code-block:: console

    $go install

You can run the ``ls -l`` command to check the modification time for the
binary to ensure the new version is installed.
