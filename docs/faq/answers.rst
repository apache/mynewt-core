FAQ
---

Here are some lists, grouped by categories, of frequently asked
questions.

**Mynewt software questions:**

-  `How do I reduce the code size for my Mynewt
   image? </os/tutorials/codesize/>`__

**Administrative questions:**

-  `How do I submit a bug? <#submit-a-bug>`__
-  `How do I request a feature? <#request-feature>`__
-  `How do I submit a patch if I am not a
   committer? <#not-committer-patch>`__
-  `Can I merge my own Pull Request into the git repo if I am a
   committer? <#committer-merge>`__
-  `How do I make changes to documentation? <#change-doc>`__
-  `How do I make changes to documentation using an editor on my
   laptop? <#doc-editor>`__

 ### How do I submit a bug? If you do not have a JIRA account sign up
for an account on
`JIRA <https://issues.apache.org/jira/secure/Signup!default.jspa>`__.

Submit a request to the @dev mailing list for your JIRA username to be
added to the Apache Mynewt (MYNEWT) project. You can view the issues on
JIRA for the MYNEWT project without an account but you need to log in
for reporting a bug.

Log in. Choose the "MYNEWT" project. Click on the "Create" button to
create a ticket. Choose "Bug" as the Issue Type. Fill in the bug
description, how it is triggered, and other details.

 How do I request a feature?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 If you do not have a JIRA account sign up for an account on
`JIRA <https://issues.apache.org/jira/secure/Signup!default.jspa>`__.

Submit a request to the @dev mailing list for your JIRA username to be
added to the Apache Mynewt (MYNEWT) project. You can view the issues on
JIRA for the MYNEWT project without an account but you need to log in
for reporting a bug.

Log in. Choose the "MYNEWT" project. Click on the "Create" button to
create a ticket. Choose "Wish" as the Issue Type. Fill in the feature
description, benefits, and any other implementation details. Note in the
description whether you want to work on it yourself.

If you are not a committer and you wish to work on it, someone who is on
the committer list will have to review your request and assign it to
you. You will have to refer to this JIRA ticket in your pull request.

I am not on the committer list. How do I submit a patch?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 **You submit your proposed changes for your peers with committer status
to review and merge.**

The process to submit a Pull Request on github.com is described on the
`Confluence page for the
project <https://cwiki.apache.org/confluence/display/MYNEWT/Submitting+Pull+Requests>`__.

I am a committer in the project. Can I merge my own Pull Request into the git repository?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 Yes, but only if your Pull Request has been reviewed and approved by
another committer in Apache Mynewt. The process to merge a Pull Request
is described on the `Confluence page for the
project <https://cwiki.apache.org/confluence/display/MYNEWT/Merging+Pull+Requests>`__.

I would like to make some edits to the documentation. What do I do?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 You submit your proposed changes for your peers with committer status
to review and merge.

Go to the `documentation
mirror <https://github.com/apache/mynewt-site>`__ on github.com.

Navigate to the file you wish to edit on github.com. All the technical
documentation is in Markdown files under the ``/docs`` directory. Click
on the pencil icon ("Edit the file in your fork of this project") and
start making changes.

Click the green "Propose file change" button. You will be directed to
the page where you can start a pull request from the branch that was
created for you. The branch is gets an automatic name ``patch-#`` where
# is a number. Click on the green "Compare & pull request" to open the
pull request.

In the comment for the pull request, include a description of the
changes you have made and why. Github will automatically notify everyone
on the commits@mynewt.apache.org mailing list about the newly opened
pull requests. You can open a pull request even if you don't think the
code is ready for merging but want some discussion on the matter.

Upon receiving notification, one or more committers will review your
work, ask for edits or clarifications, and merge when your proposed
changes are ready.

If you want to withdraw the pull request simply go to your fork
``https://github.com/<your github username>/mynewt-site`` and click on
"branches". You should see your branch under "Your branches". Click on
the delete icon.

I would like to make some edits to the documentation but want to use an editor on my own laptop. What do I do?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 You submit your proposed changes for your peers with committer status
to review and merge.

Go to the `documentation
mirror <https://github.com/apache/mynewt-site>`__ on github.com. You
need to create your own fork of the repo in github.com by clicking on
the "Fork" button on the top right. Clone the forked repository into
your laptop (using ``git clone`` from a terminal or using the download
buttons on the github page)and create a local branch for the edits and
switching to it (using ``git checkout -b <new-branchname>`` or GitHub
Desktop).

Make your changes using the editor of your choice. Push that branch to
your fork on github. Then submit a pull request from that branch on your
github fork.

The review and merge process is the same as other pull requests
described for earlier questions.
