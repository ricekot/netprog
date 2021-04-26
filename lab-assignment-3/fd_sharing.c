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
#include <fcntl.h>
#define BUF_SIZE 10 /* Maximum size of messages exchanged between client to server */
#define SV_SOCK_PATH "/tmp/server.sock"

void errExit(const char *msg)
{
    perror(msg);
    _exit(0);
}

/* Clients */
void clients(int N) {
    for (int i = 0; i < N; ++i)
    {
        if (fork() == 0)
        {
            sleep(i);
            struct sockaddr_un svaddr, claddr;
            int sfd;
            size_t msgLen;
            ssize_t numBytes;
            char buf[BUF_SIZE];
            struct stat statbuf;
            /* Create client socket; bind to unique pathname (based on PID) */
            sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
            if (sfd == -1)
                errExit("socket");
            memset(&claddr, 0, sizeof(struct sockaddr_un));
            claddr.sun_family = AF_UNIX;
            snprintf(claddr.sun_path, sizeof(claddr.sun_path),
                     "/tmp/ud_ucase_cl.%ld", (long)getpid());
            if (bind(sfd, (struct sockaddr *)&claddr, sizeof(struct sockaddr_un)) == -1)
                errExit("bind");
            /* Construct address of server */
            memset(&svaddr, 0, sizeof(struct sockaddr_un));
            svaddr.sun_family = AF_UNIX;
            strncpy(svaddr.sun_path, SV_SOCK_PATH, sizeof(svaddr.sun_path) - 1);
            /* Send messages to server; echo responses on stdout */

            msgLen = strlen(claddr.sun_path); /* May be longer than BUF_SIZE */
            if (sendto(sfd, NULL, 0, 0, (struct sockaddr *)&svaddr, sizeof(struct sockaddr_un)) == -1)
                errExit("sendto");
            numBytes = recvfrom(sfd, buf, sizeof(buf), 0, NULL, NULL);
            if (numBytes == -1)
                errExit("recvfrom");
            int recvfd = atoi(buf);
            fstat(recvfd, &statbuf);
            off_t fileSize = statbuf.st_size;
            // printf("Received %d bytes: fd %d\n", (int)numBytes, recvfd);
            char *fileBuf = malloc(sizeof(char) * fileSize/N);
            // printf("fileSize = %ld, seek: %ld\n", fileSize, i*fileSize/N);
            if (pread(recvfd, fileBuf, fileSize/N, i*fileSize/N) == -1) {
                errExit("pread");
            }
            printf("\n\033[0;33mPID: %d\n----------------\n\033[0;37m%s\n", getpid(), fileBuf);
            remove(claddr.sun_path); /* Remove client socket pathname */
            _exit(EXIT_SUCCESS);
        }
    }
}

/* Server */
int main(int argc, char *argv[])
{
    if (argc != 2 || argv[1] <= 0)
    {
        printf(
            "Usage: \033[0;31m./fd_sharing N\033[0;37m\nwhere N > 0 is "
            "the number of child processes created.\n");
        return -1;
    }

    int N = atoi(argv[1]);
    struct sockaddr_un svaddr, claddr;
    int sfd, i, j;
    ssize_t numBytes;
    socklen_t len;
    char buf[BUF_SIZE];
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

    for(;;) {
        printf("\033[0;35mEnter complete path to file: \033[0;37m");
        char path[255];
        scanf("%s", path);
        int fd = open(path, 0);
        sprintf(buf, "%d", fd);
        clients(N);
        for (i = 0; i < N; ++i)
        {
            memset(&claddr, 0, sizeof(struct sockaddr_un));
            len = sizeof(struct sockaddr_un);
            numBytes = recvfrom(sfd, buf, BUF_SIZE, 0,
                                (struct sockaddr *)&claddr, &len);
            if (numBytes == -1)
                errExit("recvfrom");
            // printf("Server received %ld bytes from %s\n", (long)numBytes, claddr.sun_path);
            
            len = sizeof(struct sockaddr_un);
            if (sendto(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&claddr, len) == -1)
                errExit("sendto");
        }
        while (wait(NULL) > 0);
    }
}

