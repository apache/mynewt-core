Known Issues
------------

Here is a list of known issues and workarounds:

1. ``newt install`` returns the following error:

   ::

       ReadDesc: No matching branch for apache-mynewt-core repo
       No matching branch for apache-mynewt-core repo 

   The apache-mynewt-core Git repository location has changed due to
   Mynewt's graduation from an incubator project to an Apache top level
   project. The HTTP redirect to the new location may fail for some
   users.

   **Workaround:** Edit the ``project.yml`` file and change the line
   ``repo: incubator-mynewt-core`` as shown in the following example to
   ``repo: mynewt-core``:

   ::

           repository.apache-mynewt-core:
               type: github
               vers: 1-latest
               user: apache
               repo: incubator-mynewt-core
