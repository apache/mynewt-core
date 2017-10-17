Upgrade a repo
--------------

In order to upgrade a previously installed repository, the "newt
upgrade" command should be issued:

::

    $ newt upgrade

Newt upgrade will look at the current desired version in
``project.yml``, and compare it to the version in ``project.state``. If
these two differ, it will upgrade the dependency. Upgrade works not just
for the dependency in ``project.yml``, but for all the sub-dependencies
that they might have.
