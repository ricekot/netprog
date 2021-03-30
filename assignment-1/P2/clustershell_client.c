#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <limits.h>

#define NODES 20
#define PORT 8080
#define BUF_SIZE 2000
#define TOK_DELIM " \t\r\n\a"

void parse_config(char *path);
void set_client_ip(char *name, int sockfd);
struct sockaddr_in connect_to_server(char *ip, int sockfd);
void display_prompt(char *name, int, struct sockaddr_in);
void send_abstract(char *prefix, char *buffer, int sockfd, struct sockaddr_in addr);
void send_input_commands(char *line, int sockfd, struct sockaddr_in addr); // CMD
void send_output(char *output, int sockfd, struct sockaddr_in addr);	   // CMD
void receive_abstract(int sockfd);										   // CMD or OUT
char *execute_command(char *command, char *input);						   // if received CMD
void display_output(char *output);										   // if received OUT

struct node
{
	char name[10];
	struct in_addr ip;
};

// Support upto NODES nodes
struct node *nodes[NODES];

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		printf("usage:\t\t./client.out <path/to/config> <name-of-this-node> <server-ip-address>\n");
		printf("example:\t./client.out ./config n2 127.0.0.1\n");
		exit(1);
	}

	char *config_path = argv[1];
	char *node_name = argv[2];
	char *server_ip = argv[3];

	parse_config(config_path);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("socket");
		printf("Error creating socket!\n");
		_exit(0);
	}
	printf("Socket created...\n");

	set_client_ip(node_name, sockfd);
	struct sockaddr_in serv_addr = connect_to_server(server_ip, sockfd);
	display_prompt(node_name, sockfd, serv_addr);
	
	return 0;
}

void parse_config(char *path)
{
	FILE *file = fopen(path, "r");
	if (file == NULL)
	{
		perror(path);
		_exit(0);
	}
	char line[256];

	for (int i = 0; fgets(line, sizeof(line), file); ++i)
	{
		/* note that fgets don't strip the terminating \n, checking its
           presence would allow to handle lines longer that sizeof(line) */
		nodes[i] = malloc(sizeof(struct node));
		strcpy(nodes[i]->name, strtok(line, TOK_DELIM));
		nodes[i]->ip.s_addr = inet_addr(strtok(NULL, TOK_DELIM));
	}
	if (!feof(file))
	{
		perror(path);
		_exit(0);
	}
	fclose(file);
}

void set_client_ip(char *name, int sockfd)
{
	struct in_addr ip;
	bool found_node = false;
	for (int i = 0; nodes[i] != NULL; ++i)
	{
		if (strcmp(nodes[i]->name, name) == 0)
		{
			ip = nodes[i]->ip;
			found_node = true;
			break;
		}
	}
	if (!found_node)
	{
		printf("Node %s is not present in the provided configuration file.\n", name);
		_exit(0);
	}
	printf("Binding to %s...\n", inet_ntoa(ip));
	// Bind to a specific network interface (and optionally a specific local port)
	struct sockaddr_in localaddr;
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr = ip;
	localaddr.sin_port = 0; // Any local port will do

	int ret = bind(sockfd, (struct sockaddr *)&localaddr, sizeof(localaddr));
	if (ret < 0)
	{
		perror("bind");
		printf("Error binding!\n");
		_exit(0);
	}
	printf("Binding done...\n");
}

struct sockaddr_in connect_to_server(char *ip, int sockfd)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = PORT;

	int ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
	{
		perror("server");
		printf("Error connecting to the server!\n");
		_exit(0);
	}
	printf("Connected to the server...\n");
	return addr;
}

void display_prompt(char *name, int sockfd, struct sockaddr_in addr)
{
	char line[LINE_MAX];
	char cwd[PATH_MAX];
	do
	{
		getcwd(cwd, sizeof(cwd));
		printf("\033[0;33m%s@clustershell", name);
		printf("\033[0;37m:");
		printf("\033[0;36m%s", cwd);
		printf("\033[0;37m$ ");

		// Exit if STDIN is closed.
		if (!fgets(line, LINE_MAX, stdin))
		{
			printf("\nExiting shell: EOF supplied.\n");
			_exit(0);
		}
		printf("%s", line);
		send_input_commands(line, sockfd, addr);
		receive_abstract(sockfd);
	} while (1);
}

void send_input_commands(char *line, int sockfd, struct sockaddr_in addr)
{
	send_abstract("CMD", line, sockfd, addr);
}

void send_output(char *output, int sockfd, struct sockaddr_in addr)
{
	send_abstract("OUT", output, sockfd, addr);
}

void send_abstract(char *prefix, char *buffer, int sockfd, struct sockaddr_in addr)
{
	char *to_send = malloc((strlen(buffer) + 4) * sizeof(char));
	strcpy(to_send, prefix);
	strcat(to_send, " ");
	strcat(to_send, buffer);
	printf("%s", to_send);
	int ret = sendto(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
	{
		perror("server");
		printf("Error sending data!\n\t-%s", buffer);
	}
}

void receive_abstract(int sockfd)
{
	char buffer[10240]; // 10 KB limit
	char input[10240];
	char output[10240];
	char command[10240];
	do
	{
		int ret = recvfrom(sockfd, buffer, BUF_SIZE, 0, NULL, NULL);
		if (ret < 0)
		{
			perror("server");
			printf("Error receiving data!\n");
			return;
		}
		char *prefix = strtok(buffer, " ");
		if (strcmp(prefix, "IN"))
		{
			strcpy(input, buffer + 4);
		}
		else if (strcmp(prefix, "OUT"))
		{
			strcpy(output, buffer + 4);
		}
		else if (strcmp(prefix, "CMD"))
		{
			strcpy(command, buffer + 4);
		}
		else
		{
			printf("Unexpected prefix %s received from server. Aborting.\n", prefix);
			return;
		}
	} while (strcmp(buffer, "FIN"));

	if (strlen(output))
	{
		execute_command(command, input);
	}
	else
	{
		display_output(output);
	}
}