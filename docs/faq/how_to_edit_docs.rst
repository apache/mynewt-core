How to Edit Docs
----------------

Objective
~~~~~~~~~

Learn the process of editing docs by adding some content to a test
document.

Markdown, MkDocs, Mou
~~~~~~~~~~~~~~~~~~~~~

The Mynewt documentation you see on the Apache website is a bunch of
HTML files generated using MkDocs which is a simple static site
generation tool geared towards building project documentation. You can
read about it at http://www.mkdocs.org. Documentation source files are
written in Markdown, and configured with a single YAML configuration
file. Markdown is a lightweight markup language with plain text
formatting syntax designed so that it can be converted to HTML and many
other formats using a tool (which in our case is MkDocs).

The HTML pages are generated periodically after changes have been
reviewed and accepted into the master branch.

Access to the Apache repo
~~~~~~~~~~~~~~~~~~~~~~~~~

Get an account on Apache. You do not need a committer account to view
the website or clone the repository but you need it to push changes to
it.

If you are not a committer, you may follow the proposed non-committer
workflow to share your work. The direct link to the proposed workflow is
https://git-wip-us.apache.org/docs/workflow.html. You will find the
steps described in more detail later in this tutorial.

Editing an existing page
~~~~~~~~~~~~~~~~~~~~~~~~

-  Create a fork on the `github
   mirror <https://github.com/apache/mynewt-site>`__.
-  Create a new branch to work on your documentation and move to that
   branch.

   ::

           $ git checkout -b <your-branch-name>

-  Make changes directly on github.com. Generate a pull request.
   Alternatively, you can edit locally on your machine, push the branch
   (or the changes in the branch) to your fork on github.com, and then
   generate a pull request.

Adding a new page
~~~~~~~~~~~~~~~~~

If you create a new file somewhere in the ``docs`` subdirectory to add a
new page, you have to add a line in the ``mkdocs.yml`` file at the
correct level. For example, if you add a new module named "Wi-Fi" by
creating a new file named ``wifi.md`` in the ``network`` directory, you
have to insert the following line under ``Networking User Guide`` in the
``mkdocs.yml`` file (at the same level as the ``docs`` directory). In
this example, a link will show up in the navigation bar on the left
under "Networking User Guide" titled "Wi-Fi" and take the user to the
contents of the 'wifi.md' file when the link is clicked. \*\* Note: The
change will show up on this `Mynewt site <http://mynewt.apache.org>`__
only after your pull request is merged in and the updated site is
generated.\*\*

::

            - 'Wi-Fi': 'wifi.md'

Local preview of HTML files
~~~~~~~~~~~~~~~~~~~~~~~~~~~

You have the option to install MkDocs and do a local conversion yourself
to preview the pages using the built-in webserver that comes with
MkDocs. In order to install MkDocs you'll need Python installed on your
system, as well as the Python package manager, pip. You can check if you
have them already (usually you will).

::

            $ python --version
            Python 2.7.2
            $ pip --version
            pip 1.5.2
            $ pip install mkdocs

You will then run the built-in webserver from the root of the
documentation directory using the command ``mkdocs serve``. The root
directory for documentation is ``mynewt-site`` or the directory with the
``mkdocs.yml`` file.

::

            $ ls
            docs        images      mkdocs.yml
            $ mkdocs serve

Then go to http://127.0.0.1:8000 to preview your pages and see how they
will look on the website. **Remember that the Myself website itself will
not be updated.**

For more information on MkDocs go to http://www.mkdocs.org.
