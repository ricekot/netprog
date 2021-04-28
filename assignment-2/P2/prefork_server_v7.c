#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <ctype.h>
#include <sys/types.h> /* Type definitions used by many programs */
#include <stdio.h>     /* Standard I/O functions */
#include <stdlib.h>    /* Prototypes of commonly used library functions, plus EXIT_SUCCESS and EXIT_FAILURE constants */
#include <unistd.h>    /* Prototypes for many system calls */
#include <errno.h>     /* Declares errno and defines error constants */
#include <string.h>    /* Commonly used string-handling functions */
#include <stdbool.h>   /* 'bool' type plus 'true' and 'false' constants */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>
#define BUF_SIZE 100 /* Maximum size of messages exchanged between client to server */
#define SV_SOCK_PATH "/tmp/prefork_sv.sock"
#define PORT "4000" /* Name of TCP service */
#define LOCALHOST "127.0.0.1"

struct child
{
    pid_t pid;
    bool idle;
    long reqsHandled;
};

void errExit(const char *msg)
{
    perror(msg);
    _exit(0);
}

static int iPow(int a, int b)
{
    if (b < 0)
        return 0;
    else if (b == 0)
        return 1;
    else if (b == 1)
        return a;
    else
        return a * iPow(a, b - 1);
}

/* Handle a client request: send dummy value */
static void
handleRequest(int cfd)
{
    char buf[BUF_SIZE];
    ssize_t numRead;

    while ((numRead = read(cfd, buf, BUF_SIZE)) > 0)
    {
        snprintf(buf, BUF_SIZE, "%d", rand());
        if (write(cfd, buf, BUF_SIZE) != -1)
        {
            errExit("write()");
        }
    }

    if (numRead == -1)
    {
        errExit("read()");
    }
}

static void /* SIGCHLD handler to reap dead child processes */
grimReaper(int sig)
{
    int savedErrno; /* Save 'errno' in case changed here */

    savedErrno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        continue;
    errno = savedErrno;
}

int inetListen(char *bind_addr, int port)
{
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    int option = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    if (sockfd == -1)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if (inet_aton(bind_addr, &sa.sin_addr) == 0)
    {
        perror("inet_aton");
        close(sockfd);
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        perror("bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 5) == -1)
    {
        perror("listen");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/**
* Fork child process, establish a Unix Domain Datagram
* connection with parent and accept TCP connections.
*/
int spawnChild(int lfd)
{
    int pid = fork();
    if (pid == -1)
    {
        errExit("fork");
    }
    else if (pid > 0)
    {
        return pid;
    }
    else if (pid == 0)
    {
        struct sockaddr_un svaddr, claddr;
        int sfd, cfd;
        size_t msgLen;
        ssize_t numBytes;
        char buf[BUF_SIZE];
        struct stat statbuf;
        int requestsHandled = 0;
        /* Create client socket; bind to unique pathname (based on PID) */
        sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (sfd == -1)
            errExit("socket");
        memset(&claddr, 0, sizeof(struct sockaddr_un));
        claddr.sun_family = AF_UNIX;
        snprintf(claddr.sun_path, sizeof(claddr.sun_path), "/tmp/prefork_ch.%ld", (long)getpid());
        if (bind(sfd, (struct sockaddr *)&claddr, sizeof(struct sockaddr_un)) == -1)
            errExit("bind");
        /* Construct address of server */
        memset(&svaddr, 0, sizeof(struct sockaddr_un));
        svaddr.sun_family = AF_UNIX;
        strncpy(svaddr.sun_path, SV_SOCK_PATH, sizeof(svaddr.sun_path) - 1);

        for (;;)
        {
            cfd = accept(lfd, NULL, NULL); /* Wait for connection */
            if (cfd == -1)
            {
                errExit("accept");
            }

            ++requestsHandled;
            sprintf(buf, "%d", getpid());
            if (sendto(sfd, buf, strlen(buf), 0, (struct sockaddr *)&svaddr, sizeof(struct sockaddr_un)) == -1)
                errExit("sendto");
            if (recvfrom(sfd, buf, BUF_SIZE, 0, NULL, NULL) == -1)
                errExit("recvfrom");
            if (!strcpy(buf, "johncena"))
            {
                printf("Exiting %d\n", getpid());
                remove(claddr.sun_path); /* Remove client socket pathname */
                _exit(0);
            }

            close(lfd); /* Unneeded copy of listening socket */
            handleRequest(cfd);
            close(cfd);
        }
        return 0;
    }
}

/* Server */
int main(int argc, char *argv[])
{
    if (argc != 4 || argv[1] < 0 || argv[2] <= 0 || argv[3] <= 0)
    {
        printf("USAGE: ./prefork_server minSpareServers maxSpareServers maxRequestsPerChild\n");
        return -1;
    }

    // Seed for rand() used for random data
    srand(time(0));

    int minSpareServers = atoi(argv[1]);
    int maxSpareServers = atoi(argv[2]);
    int maxReqsPerChild = atoi(argv[3]);
    struct child *cptr;
    struct sockaddr_un svaddr, claddr;
    int sfd, i, j, numSpareServers, cpid, cReqsHandled, cptrLength;
    ssize_t numBytes;
    socklen_t len;
    char buf[BUF_SIZE];
    char *token;
    sfd = socket(AF_UNIX, SOCK_DGRAM, 0); /* Create server socket */
    if (sfd == -1)
        errExit("socket");
    /* Construct well-known address and bind server socket to it */
    if (remove(SV_SOCK_PATH) == -1 && errno != ENOENT)
        errExit(strcat("remove-", SV_SOCK_PATH));
    memset(&svaddr, 0, sizeof(struct sockaddr_un));
    svaddr.sun_family = AF_UNIX;
    strncpy(svaddr.sun_path, SV_SOCK_PATH, sizeof(svaddr.sun_path) - 1);
    if (bind(sfd, (struct sockaddr *)&svaddr, sizeof(struct sockaddr_un)) == -1)
        errExit("bind");

    for (;;)
    {
        int lfd; /* Listening socket for TCP connections*/
        struct sigaction sa;

        /* Establish SIGCHLD handler to reap terminated child processes */
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sa.sa_handler = grimReaper;
        if (sigaction(SIGCHLD, &sa, NULL) == -1)
        {
            errExit("sigaction");
        }

        lfd = inetListen(LOCALHOST, INADDR_ANY);
        if (lfd == -1)
        {
            errExit("inetListen");
        }

        // Start maxSpareServers servers
        cptr = calloc(maxSpareServers, sizeof(struct child));
        cptrLength = maxSpareServers;
        numSpareServers = maxSpareServers;
        for (i = 0; i < numSpareServers; ++i)
        {
            cptr[i].pid = spawnChild(lfd);
            cptr[i].reqsHandled = 0;
            cptr[i].idle = true;
        }

        for (;;)
        {
            memset(&claddr, 0, sizeof(struct sockaddr_un));
            len = sizeof(struct sockaddr_un);
            if (recvfrom(sfd, buf, BUF_SIZE, 0, (struct sockaddr *)&claddr, &len) == -1)
                errExit("recvfrom");

            cpid = atoi(buf);
            printf("Received PID = %d\n", cpid);

            for (i = 0; i < numSpareServers; ++i)
            {
                if (cpid == cptr[i].pid)
                {
                    cptr[i].reqsHandled++;
                    cptr[i].idle = false;
                    numSpareServers--;
                    if (cptr[i].reqsHandled >= maxReqsPerChild)
                    {
                        strcpy(buf, "johncena");
                        cptr[i].pid = -1;
                    }
                }
            }

            len = sizeof(struct sockaddr_un);
            if (sendto(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&claddr, len) == -1)
                errExit("sendto");

            if (numSpareServers < minSpareServers)
            {
                cptr = (struct child *)realloc(cptr, (numSpareServers - minSpareServers) * sizeof(struct child));
                for (i = 0; numSpareServers <= minSpareServers; ++i)
                {
                    sleep(1);
                    for (j = 0; j < iPow(2, i) && numSpareServers < minSpareServers; ++j)
                    {
                        cptr[cptrLength - 1].pid = spawnChild(lfd);
                        cptr[cptrLength - 1].reqsHandled = 0;
                        cptr[cptrLength - 1].idle = true;
                        cptrLength++;
                        numSpareServers++;
                    }
                }
            }

            if (numSpareServers > maxSpareServers)
            {
                for (i = 0; i < numSpareServers && numSpareServers > maxSpareServers; ++i)
                {
                    if (cptr[i].idle)
                    {
                        kill(cptr[i].pid, SIGINT);
                        numSpareServers--;
                    }
                }
            }
        }
        while (wait(NULL) > 0)
            ;
    }
}
