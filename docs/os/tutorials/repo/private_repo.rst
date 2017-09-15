Accessing a private repository
------------------------------

To access a private repository, newt needs to be configured with one of
the following:

-  Access token for the repository
-  Basic auth login and password for the user

**NOTE:** To create a github access token, see
https://help.github.com/articles/creating-an-access-token-for-command-line-use/

There are two ways to specify this information, as shown below. In these
examples, both a token and a login/password are specified, but you only
need to specify one of these.

1. project.yml (probably world-readable and therefore not secure):

``hl_lines="6 7 8"     repository.my-private-repo:         type: github         vers: 0-dev         user: owner-of-repo         repo: repo-name         token: '8ab6433f8971b05c2a9c3341533e8ddb754e404e'         login: githublogin         password: githubpassword``

2. $HOME/.newt/repos.yml

``hl_lines="2 3 4"     repository.my-private-repo:         token: '8ab6433f8971b05c2a9c3341533e8ddb754e404e'         login: githublogin         password: githubpassword``

If both a token and a login+password are specified, newt uses the token.
If both the project.yml file and the private repos.yml file specify
security credentials, newt uses the project.yml settings.

**NOTE:** When newt downloads the actual repo content, as opposed to
just the repository.yml file, it does not use the same mechanism.
Instead, it invokes the git command line tool. This is an annoyance
because the user cannot use the same access token for all git
operations. This is something that will be fixed in the future.
