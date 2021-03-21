# P1. Shell

You are required to build a bash-like shell for the following requirements. Your program should not use temporary files, `popen()`, `system()` library calls. It should only use system-call wrappers from the library. It should not use `sh` or `bash` shells to execute a command.
1. Shell should wait for the user to enter a command. User can enter a command with multiple arguments. Program should parse these arguments and pass them to `execv()` call. For every command, shell should search for the file in PATH and print any error. Shell should also print the pid, status of the process before asking for another command. 
1. shell should create a new process group for every command. When a command is run with `&` at end, it is counted as background process group. Otherwise it should be run as foreground process group (look at `tcsetpgrp()`). That means any signal generated in the terminal should go only to the command running, not to the shell process. 
1. shell should support `<`, `>`, and `>>` redirection operators. Print details such as fd of the file, remapped fd. 
1. shell should support any number of commands in the pipeline. e.g. `ls|wc|wc|wc`. Print details such as pipe fds, process pids and the steps. Redirection operators can be used in combination with pipes. 
1. shell should support two new pipeline operators `||` and `|||`. E.g.: `ls -l || grep ^-, grep ^d`. It means that output of ls -l command is passed as input to two other commands. Similarly, `|||` means, output of one command is passed as input to three other commands separated by `,`. 
2. shell should support a mode called 'short-cut commands' executed by command `sc`. In this mode, a command can be executed by pressing Ctrl-C and pressing a number. This number corresponds to the index in the look up table created and deleted by the commands `sc -i index cmd` and `sc -d index cmd`. 

Deliverables:
- Brief Design Document (.pdf)
- shell.c
