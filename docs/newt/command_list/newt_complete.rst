newt complete 
--------------

Performs bash autocompletion using tab. It is not intended to be called
directly from the command line.

Install bash autocompletion
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

        $ brew install bash-completion
        Updating Homebrew...
        <snip>
        Bash completion has been installed to:
          /usr/local/etc/bash_completion.d
        ==> Summary
        🍺  /usr/local/Cellar/bash-completion/1.3_1: 189 files, 607.8K

Enable autocompletion for newt
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

        $ complete -C "newt complete" newt

Usage
^^^^^

Hit tab and see possible completion options or completed command.

.. code-block:: console

        $ newt target s
        set   show  
        $ newt target show
