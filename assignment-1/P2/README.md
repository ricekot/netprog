# P2. Cluster Shell

In this problem you are required to extend the shell features to a cluster of machines, each identified by a name. The name to ip mapping is available in a config file, whose path is specified at the start of the shell. Assume that N nodes in the cluster are named as *n1*, *n2*, ..., *nN*.

- Cluster shell is run on any one of the nodes. When a command is run e.g. ls it executed on the local system. When `n2.ls` is run in *n1*, it is executed on node *n2* and the output is listed on *n1*. When `n*.ls` is run on *n1*, `ls` is run on all nodes and output is displayed on *n1*. This applies to other commands as well. By default, all commands on a remote node are executed in the home directory of the user logged in *n1*. That is, it is necessary to have the same user on all systems. When `n2.cd path` or `n*.cd path` is executed, directory is changed. 
- When the command `n1.cat file|n2.sort|n3.uniq` is executed on *n5*, the commands are executed on different nodes taking input from the previous command but the last output is displayed on the node *n5* it is executed on. 
- The command `nodes` display the list of nodes (name, ip) currently active in the cluster. 

Deliverables:
- clustershell_client.c, clustershell_server.c
- pdf file explaining design decisions
