#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "commonProto.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include<signal.h>


char buf[1024];
int alarmed = 1;
unsigned int timeout;

void signal_handler(){
    alarmed = 0;
}

void *Thread_Send(void* sockfd){
    int length;
    int id = *(int*) sockfd;
    if ((length = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
        buf[length] = '\0';
        send(id, buf, strlen(buf), 0);
        alarm(timeout);
    }
    else{
        close(id);
        exit(0);
    }
}

void *Thread_Receive(void * sockfd){
    int id = *(int*) sockfd;
    recv(id, &buf, sizeof buf, 0);
    alarm(timeout);
    printf("%s", buf);
}

int main(int argc, char **argv) {

    // This is some sample code feel free to delete it
    // This is the main program for the thread version of nc

    struct commandOptions cmdOps;
    int retVal = parseOptions(argc, argv, &cmdOps);
    if(retVal == PARSE_ERROR) {
        usage(argv[0]);
        return -1;
    }
    printf("Command parse outcome %d\n", retVal);

    printf("-k = %d\n", cmdOps.option_k);
    printf("-l = %d\n", cmdOps.option_l);
    printf("-v = %d\n", cmdOps.option_v);
    printf("-r = %d\n", cmdOps.option_r);
    printf("-p = %d\n", cmdOps.option_p);
    printf("-p port = %u\n", cmdOps.source_port);
    printf("-w  = %d\n", cmdOps.option_w);
    printf("Timeout value = %u\n", cmdOps.timeout);
    printf("Host to connect to = %s\n", cmdOps.hostname);
    printf("Port to connect to = %u\n", cmdOps.port);

    struct addrinfo hints, *res;
    int yes = 1;

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));


    if(cmdOps.option_l) {   //Start as server
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        char port[5];
        sprintf(port, "%u", cmdOps.port);
        getaddrinfo(cmdOps.hostname, port, &hints, &res);
        bind(sockfd, res->ai_addr, res->ai_addrlen);
        listen(sockfd, 5);
        printf("server: waiting for connections...\n");

    }
    else{    // Start as client
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if (cmdOps.option_p) {
            char port[5];
            sprintf(port, "%u", cmdOps.port);
            getaddrinfo(cmdOps.hostname, port, &hints, &res);
        }
        else{
            getaddrinfo(cmdOps.hostname, "8888", &hints, &res);
        }


        //bind(sockfd, res->ai_addr, res->ai_addrlen);

        if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
            close(sockfd);
            perror("Connection failed");
            exit(-1);
        }

        pthread_t thread_send;
        pthread_t thread_receive;

        if(cmdOps.option_w == 1) timeout = cmdOps.timeout;
        alarm(timeout);
        while(alarmed) {
            signal(SIGALRM,signal_handler);
            if (pthread_create(&thread_send, NULL, Thread_Send, &sockfd) != 0) {
                perror("Unable to send");
            }
            if (pthread_create(&thread_receive, NULL, Thread_Receive, &sockfd) != 0) {
                perror("Unable to receive");
            }
        }
        close(sockfd);
        exit(0);

    }
}
