#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>

#define RL_BUFSIZE 1024
#define TOK_BUFSIZE 64
#define TOK_DELIM " \t\r\n\a"

// Declaring functions
char **parse_input(char *, int *);
char *read_line(void);
int shell_execute(int, char **, int, int);
int shell_run(int, char **);
void sighandler(int);

sig_atomic_t shortcut_mode_enabled = 0;
char *shortcuts[10];

// Main function
int main(int argc, char **argv) {
	printf("\e[1;1H\e[2J");
	printf("\033[0;36m  _____            \033[0;33m  _____ _          _ _ \n");
	printf("\033[0;36m / ____|           \033[0;33m / ____| |        | | |\n");
	printf("\033[0;36m| (___   ___  __ _ \033[0;33m| (___ | |__   ___| | |\n");
	printf("\033[0;36m \\___ \\ / _ \\/ _` |\033[0;33m \\___ \\| '_ \\ / _ \\ | |\n");
	printf("\033[0;36m ____) |  __/ (_| |\033[0;33m ____) | | | |  __/ | |\n");
	printf("\033[0;36m|_____/ \\___|\\__,_|\033[0;33m|_____/|_| |_|\\___|_|_|\n");  
	printf("\033[0;37m");                                          
	printf("\nSeaShell: A simple Unix Shell in C.\n");
	printf("\nThis was made as part of Assignment - 1 for the course IS F462 - Network Programming at BITS Pilani by,\n\n");
	printf("Akshath Kothari - 2018B4A40607P\n");
	printf("Shivam Agarwal  - 2018B3A70786P\n");
	printf("Shrey Aggarwal  - 2018B5A80923P\n");
	printf("\nType 'help' for more information about this shell.\n\n");
	
	char *line;
	char **input_argv;
	int status;
	// sighandler for shortcut commands
	signal(SIGINT, sighandler);

	do {
		char cwd[PATH_MAX];
		getcwd(cwd, sizeof(cwd));
		printf("\033[0;33mshell");
		printf("\033[0;37m:");
		printf("\033[0;36m%s", cwd);
		printf("\033[0;37m$ ");
		
		line = read_line();
		// Exit if STDIN is closed.
		if (line == NULL){
			printf("\nExiting shell: EOF supplied.\n");
			return 0;
		}
		if (shortcut_mode_enabled) {
			char c = line[0];
			int index = c - '0';
			if (index < 0 || index > 9) {
				printf("sc: invalid index %c\n", c);
			}
			else if (shortcuts[index] == NULL) {
				printf("sc: no command at index %c\n", c);
			}
			else {
				char *argv[1];
				argv[0] = shortcuts[index];
				shell_run(1, argv);
			}
			shortcut_mode_enabled = 0;
		}
		else {
			int input_argc = 0;
			input_argv = parse_input(line, &input_argc);
			status = shell_run(input_argc, input_argv);
			free(input_argv);
		}

		free(line);
	}
	while (status);

	return EXIT_SUCCESS;
}

// Reading input character by character without readline().
char *read_line(void) {
	int bufsize = RL_BUFSIZE;
	int position = 0;
	char *buffer = malloc(sizeof(char) * bufsize);
	int c;

	if (!buffer) {
		perror("shell: allocation error.\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		// Read a character from the input
		c = getchar();

		switch(c) {
			case EOF:
				return NULL;
			case '\n':
				buffer[position++] = '\0';
				return buffer;
			case '<': case '>': case '|': case ',':
				buffer[position++] = ' ';
				buffer[position++] = c;
				char next;
				int i = 0;
				while ((next = getchar()) == c && i++ < 2) {
					buffer[position++] = c;
				}
				buffer[position++] = ' ';
				if (next == EOF || next == '\n'){
					buffer[position++] = next;
					return buffer;
				}
				buffer[position++] = next;
				break;
			default: 
				buffer[position++] = c;
				break;
		}

		// If we exceed buffer, allocate more space
		if (position >= bufsize) {
			bufsize += RL_BUFSIZE;
			buffer = realloc(buffer, bufsize);
			if (!buffer) {
				perror("shell: allocation error.\n");
				exit(EXIT_FAILURE);
			}
		}
	}
	return buffer;
}

// Parse input, update argc, and return argv
char **parse_input(char *line, int *argc) {
	// Check that argc is not pointing to NULL
	assert(argc);

	int bufsize = TOK_BUFSIZE, position = 0;
	char **argv = malloc(bufsize * sizeof(char*));
	char *token;

	if (!argv) {
		perror("shell: allocation error.\n");
		exit(EXIT_FAILURE);
	}

	// Get the first token in the line entered.
	token = strtok(line, TOK_DELIM);
	while (token != NULL) {
		argv[position] = token;
		position++;

		// Expand buffer size if input exceeds the amount of allocated memory.
		if (position >= bufsize) {
			bufsize += TOK_BUFSIZE;
			argv = realloc(argv, bufsize * sizeof(char*));
			if (!argv) {
				perror("shell: allocation error.\n");
				exit(EXIT_FAILURE);
			}
		}
		// Get newtoken by taking the string from where the first delimiter is found after the previous call.
		// strtok() stores a pointer in a static variable so it remembers the string that is entered the first time.
		// Hence, everytime we call it with NULL, it continues from where it left off.
		token = strtok(NULL, TOK_DELIM);
	}

	*argc = position;
	// Set last token as NULL.
	argv[position] = NULL;
	return argv;
}

// Declare built-in functions
int shell_cd(char **args);
int shell_sc(char **args);
int shell_help(char **args);
int shell_exit(char **args);

// Array of function pointers that takes an array of strings and returns an int.
int (*builtin_func[]) (char **) = {
	&shell_cd,
	&shell_sc,
	&shell_help,
	&shell_exit
};

// List of builtin commands followed by their functions
char *builtin_str[] = {
	"cd",
	"sc",
	"help",
	"exit"
};

// Returns the number of builtin functions
int shell_num_builtins() {
	return sizeof(builtin_str) / sizeof(char *);
}

// Recursively parse and execute commands
int shell_recur(int argc, char **argv, int in, int out) {
	char **input_argv = malloc(argc * sizeof(char *));
	
	int i, j;
	for (i = 0, j = 0; i < argc && j <= i; ++i, ++j) {
		char *tok = argv[i];
		if (!(strcmp(tok, ",,") && strcmp(tok, ",,,") && strcmp(tok, "<<") && strcmp(tok, "<<<") && strcmp(tok, ">>>"))) {
			printf("\n%s is not supported.\n", tok);
			return 1;
		}

		input_argv[j] = (char *) malloc(TOK_BUFSIZE);
		strcpy(input_argv[j], tok);

		if (strcmp(tok, "|") == 0) {
			int p[2];
			pipe(p);
			input_argv[j] = NULL;
			shell_execute(j, input_argv, in, p[1]);
			char **new_argv = malloc((argc - i) * sizeof(char *));
			for (int k = i + 1; k <= argc; ++k) {
				new_argv[k - i - 1] = argv[k];
			}
			printf("[pipe] [%s | %s] [FD_in %d] [FD_out %d]\n", input_argv[0], new_argv[0], p[0], p[1]);
			return shell_recur(argc - i - 1, new_argv, p[0], out);
		}
		else if (strcmp(tok, "||") == 0) {
			int p1[2], p2[2];
			pipe(p1); pipe(p2);
			input_argv[j] = NULL;
			shell_execute(j, input_argv, in, p1[1]);

			// Copy pipe data using tee
			if (tee(p1[0], p2[1], INT_MAX, SPLICE_F_NONBLOCK) < 0) {
				perror("tee");
			}
			close(p2[1]);

			char **new_argv_1 = malloc((argc - i) * sizeof(char *));
			char **new_argv_2 = malloc((argc - i) * sizeof(char *));
			
			int k;
			for (k = i + 1; k < argc && strcmp(argv[k], ","); ++k) {
				new_argv_1[k - i - 1] = argv[k];
			}
			new_argv_1[k - i - 1] = NULL;
			
			for (int m = k + 1; m <= argc; ++m){
				new_argv_2[m - k - 1] = argv[m];
			}

			printf("[pipe] [%s | %s] [FD_in %d] [FD_out %d]\n", input_argv[0], new_argv_1[0], p1[0], p1[1]);
			printf("[pipe] [%s | %s] [FD_in %d] [FD_out %d]\n", input_argv[0], new_argv_2[0], p2[0], p2[1]);
			return shell_recur(k - i - 1, new_argv_1, p1[0], out)
				&& shell_recur(argc - k - 1, new_argv_2, p2[0], out);
		}
		else if (strcmp(tok, "|||") == 0) {
			int p1[2], p2[2], p3[2];
			pipe(p1); pipe(p2); pipe(p3);
			input_argv[j] = NULL;
			shell_execute(j, input_argv, in, p1[1]);

			// Copy pipe data using tee
			if (tee(p1[0], p2[1], INT_MAX, SPLICE_F_NONBLOCK) < 0 || tee(p1[0], p3[1], INT_MAX, SPLICE_F_NONBLOCK) < 0) {
				perror("tee");
			}
			close(p2[1]); close(p3[1]);

			char **new_argv_1 = malloc((argc - i) * sizeof(char *));
			char **new_argv_2 = malloc((argc - i) * sizeof(char *));
			char **new_argv_3 = malloc((argc - i) * sizeof(char *));
			
			int k;
			for (k = i + 1; k < argc && strcmp(argv[k], ","); ++k) {
				new_argv_1[k - i - 1] = argv[k];
			}
			new_argv_1[k - i - 1] = NULL;
			
			int m;
			for (m = k + 1; m < argc && strcmp(argv[m], ","); ++m) {
				new_argv_2[m - k - 1] = argv[m];
			}
			new_argv_2[m - k - 1] = NULL;

			for (int n = m + 1; n <= argc; ++n){
				new_argv_3[n - m - 1] = argv[n];
			}

			printf("[pipe] [%s | %s] [FD_in %d] [FD_out %d]\n", input_argv[0], new_argv_1[0], p1[0], p1[1]);
			printf("[pipe] [%s | %s] [FD_in %d] [FD_out %d]\n", input_argv[0], new_argv_2[0], p2[0], p2[1]);
			printf("[pipe] [%s | %s] [FD_in %d] [FD_out %d]\n", input_argv[0], new_argv_3[0], p3[0], p3[1]);
			return shell_recur(k - i - 1, new_argv_1, p1[0], out)
				&& shell_recur(m - k - 1, new_argv_2, p2[0], out)
				&& shell_recur(argc - m - 1, new_argv_3, p3[0], out);
		}
		else if (strcmp(tok, "<") == 0) {
			j--; i++;
			in = open(argv[i], O_RDONLY);
			printf("[redirect in] [FD %d %s]\n", in, argv[i]);
		}
		else if (strcmp(tok, ">") == 0) {
			j--; i++;
			out = creat(argv[i], S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP);
			printf("[redirect out] [%s] [FD %d]\n", argv[i], out);
		}
		else if (strcmp(tok, ">>") == 0) {
			j--; i++;
			out = open(argv[i], O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP);
			printf("[redirect out] [%s] [FD %d]\n", argv[i], out);
		} 
	}
	input_argv[j] = NULL;
	return shell_execute(j, input_argv, in, out);
}

// Convenience Function
int shell_run(int argc, char **argv) {
	return shell_recur(argc, argv, -1, -1);
}

// Executes parsed input
int shell_execute(int argc, char **argv, int in, int out) {
	int i;

	int stdin_fd_copy, stdout_fd_copy;
	if (in != -1) {
		stdin_fd_copy = dup(STDIN_FILENO);
		dup2(in, STDIN_FILENO);
		close(in);
	}
	if (out != -1) {
		stdout_fd_copy = dup(STDOUT_FILENO);
		dup2(out, STDOUT_FILENO);
		close(out);
	}

	if (argv[0] == NULL) {
		// Empty command was entered
		return 1;
	}

	// Check if command is built-in first
	for (i = 0; i < shell_num_builtins(); i++) {
		if (strcmp(argv[0], builtin_str[i]) == 0) {
			return (*builtin_func[i])(argv);
		}
	}

	bool run_in_bg = false;
	if (strcmp(argv[argc - 1], "&") == 0) {
		run_in_bg = true;
		argv[argc - 1] = NULL;
	}

	pid_t pid;
	int status;
	pid = fork();
	if (pid > 0) {
		// Parent process
		
		if (run_in_bg) {
			// Run child process in background
			// Happens automatically because a new process group is created by the child
			pid_t bg_status = waitpid(pid, &status, WNOHANG);
			if (bg_status == 0) {
				printf("[process] [%s] [PID %d] [Running in background]\n", argv[0], pid);
			} else if (bg_status == -1) {
				perror("Something went wrong while executing command in background.");
			}
		}
		else {
			// Run child process in foreground
			if (tcsetpgrp(STDIN_FILENO, pid) == -1 && in == -1) {
				perror("shell: could not run command in foreground");
			}

			// Wait for child to exit
			waitpid(pid, &status, WUNTRACED);
			if (in != -1) {
				dup2(stdin_fd_copy, STDIN_FILENO);
				close(stdin_fd_copy);
			}
			if (out != -1) {
				dup2(stdout_fd_copy, STDOUT_FILENO);
				close(stdout_fd_copy);
			}
			
			printf("[process] [%s] [PID %d] ", argv[0], pid);
			if (WIFEXITED(status)) {
				printf("[Exited Normally]\n");
			} else if (WIFSIGNALED(status) || WIFSTOPPED(status)) {
				printf("[Terminated by a signal]\n");
			}

			// Bring shell to foreground
			signal(SIGTTOU, SIG_IGN);
			if (tcsetpgrp(STDIN_FILENO, getpid()) == -1) {
				perror("shell: could not bring shell to foreground");
			}
			signal(SIGTTOU, SIG_DFL);
		}
	}
	else if (pid == 0) {
		// Child process
		// Create a new process group and set pgid as pid of this process
		setpgid(0, 0);

		// Never returns if call is successful
		// If -1 is returned, execution has failed.
		if (execvp(argv[0], argv) < 0){
			perror(argv[0]);
			// This exit call only terminates the child process
			exit(1);
		}
	}
	// If OS runs out of memory or reaches the max number of allowed processes, a child process will not be created and it will return -1.
	else {
		perror("shell: fork Failed.\n");
		// This exit call terminates the entire program.
		exit(1);
	}
	return 1;
}

// Making the shell support cd
// cd is not a system program and even if it does work, it's executed in a child process due to how the program flow is in a shell.
// As a result, in the main process, we are still in the current directory.
int shell_cd(char **args) {
	if (chdir(args[1]) != 0) {
		perror("cd");
	}
	return 1;
}

// Shortcut commands
int shell_sc(char **argv){
	if (argv[1] == NULL || argv[2] == NULL || argv[3] == NULL) {
		printf("sc: Expected more arguments\n");
		return 1;
	}
	char *option = argv[1];
	int index = atoi(argv[2]);
	char *cmd = argv[3];

	if (index < 0 || index > 9) {
		printf("sc: Index must lie in [0, 9]\n");
		return 1;
	}
	if (!strcmp(option, "-i")) {
		shortcuts[index] = cmd;
	} else if (!strcmp(option, "-d")){
		shortcuts[index] = NULL;
	} else {
		printf("sc: Unsupported option '%s'\n", option);
	}
	return 1;
}

void sighandler(int signum) {
    switch(signum){
        case SIGINT:
			shortcut_mode_enabled = 1;
			printf("\n[SHORTCUT MODE] Enter a shortcut (0 - 9): ");
			fflush(stdout);
			break;
    }
}

// help command implementation
int shell_help(char **args) {
	int i;
	printf("SeaShell: A simple Unix Shell in C.\n");
	printf("This was made as part of an assignment for the course IS F462 - Network Programming at BITS Pilani.\n\n");
	printf("Type program names and arguments and hit enter.\n");
	printf("This shell supports redirection (<, >, >>), piping (|, ||, |||) and shortcuts (sc).\n");
	printf("Use the man command for information on other programs.\n");
	return 1;
}

// exit command implementation
int shell_exit(char **args) {
	return 0;
}
