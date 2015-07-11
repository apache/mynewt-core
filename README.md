# Stack Test Repository 

# Overview

This is a test repository containing the full stack repository, to make a working 
set of compiled images 

# Sub-Trees

This repository has links to other packages distributed by Micosa, examples 
include: 

* OS (http://github.com/micosa/os)
* CMSIS-CORE (http://github.com/micosa/cmsis-core)

These packages are imported into this repository using git subtrees.  To see a 
full set of remote repositories, look at the setup-remotes.sh script, which 
adds the remote repositories.

In order to update these repositories, git subtree merging should be employed, 
where changes are made locally and tested within the dev_test repository first, 
and then pushed upstream once tested. 
