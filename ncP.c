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

char buf[1024];

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count){
    pfds[i] = pfds[*fd_count-1];
    (*fd_count)--;
}

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count)
{
    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN;
    (*fd_count)++;
}

void recv_message(int sockfd){
    int len;
    memset(&buf, 0, sizeof buf);
    if((len = recv(sockfd, &buf, sizeof buf, 0)) < 0){
        perror("receive error");
        close(sockfd);
        exit(1);
    }
    else printf("%s", buf);

    while(len == sizeof buf){
        memset(&buf, 0, sizeof buf);
        len = recv(sockfd, buf, strlen(buf), 0);
    }
}

void send_message(int sockfd){
    int len;
    if ((len = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
        send(sockfd, buf, strlen(buf), 0);
    }
    else{
        close(sockfd);
        exit(0);
    }
    while(len == sizeof buf){
        memset(&buf, 0, sizeof buf);
        len = read(STDIN_FILENO, buf, sizeof buf);
        send(sockfd, buf, strlen(buf), 0);
    }
}


int main(int argc, char **argv) {

  // This is some sample code feel free to delete it
  // This is the main program for the thread version of nc

  struct commandOptions cmdOps;
  int retVal = parseOptions(argc, argv, &cmdOps);
    if(retVal != PARSE_OK) {
        usage(argv[0]);
        return -1;
    }

    struct addrinfo hints, *res;

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    if(cmdOps.option_l) {   //Start as server

        char port[5];
        sprintf(port, "%u", cmdOps.port);
        getaddrinfo(cmdOps.hostname, port, &hints, &res);
        bind(sockfd, res->ai_addr, res->ai_addrlen);
        listen(sockfd, 20);
        printf("server: waiting for connections...\n");

            int fd_size = 3;
            if(cmdOps.option_r) fd_size = 7;
            int fd_count = 2;

            struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

            pfds[0].fd = sockfd;
            pfds[0].events = POLLIN;

            pfds[1].fd = STDIN_FILENO;
            pfds[1].events = POLLIN;

            while(1) {
                poll(pfds, fd_count, -1);

                for (int i = 0; i < fd_count; i++) {
                    if (pfds[i].revents & POLLIN) {
                        //Accept the new connection
                        if (pfds[i].fd == sockfd) {
                            struct sockaddr_in clientAddress;
                            socklen_t size = sizeof(clientAddress);
                            if (fd_count < fd_size) {
                                int clientfd;
                                if ((clientfd = accept(sockfd, (struct sockaddr *) &clientAddress, &size)) < 0) {
                                    perror("server: fail connect");
                                }
                                char dst[INET_ADDRSTRLEN];
                                inet_ntop(clientAddress.sin_family, &clientAddress.sin_addr, dst, sizeof dst);

                                printf("server: new connection from %s:%d.\n", dst, ntohs(clientAddress.sin_port));
                                add_to_pfds(&pfds, clientfd, &fd_count);
                            }
                        }
                        //Standard input
                        else if (pfds[i].fd == STDIN_FILENO) {
                            int length;
                            memset(&buf, 0, sizeof buf);
                            if ((length = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
                                for (int j = 2; j < fd_count; j++) {
                                    if (send(pfds[j].fd, buf, length, 0) == -1) {
                                        perror("sending error");
                                    }
                                }
                            } else {
                                 close(pfds[i].fd);
                                 del_from_pfds(pfds, i, &fd_count);
                            }
                        }
                        //Receive message
                        else {
                            memset(&buf, 0, sizeof buf);
                            int sender = pfds[i].fd;
                            int length = recv(pfds[i].fd, &buf, sizeof buf, 0);
                            printf("%s", buf);

                            if (length <= 0) {
                                if (length < 0) perror("receive error");

                                close(pfds[i].fd);
                                del_from_pfds(pfds, i, &fd_count);

                            } else {
                                buf[length] = '\0';
                                for (int j = 2; j < fd_count; j++) {
                                    // Send to everyone!
                                    int dest_fd = pfds[j].fd;

                                    // Except the listener and ourselves
                                    if (dest_fd != sockfd && dest_fd != sender) {
                                        if (send(dest_fd, buf, length, 0) == -1) {
                                            perror("send");
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (cmdOps.option_k && fd_count == 2) printf("server: waiting for connections...\n");
                    else if (!cmdOps.option_k && fd_count == 2) {
                        close(sockfd);
                        exit(0);
                    }
                }
            }
    }
    else{    // Start as client
            if (cmdOps.option_p) {
                char port[5];
                sprintf(port, "%u", cmdOps.port);
                getaddrinfo(cmdOps.hostname, port, &hints, &res);
            }
            else{
                getaddrinfo(cmdOps.hostname, "8888", &hints, &res);
            }
            bind(sockfd, res->ai_addr, res->ai_addrlen);

            if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
                close(sockfd);
                perror("Connection failed");
                exit(-1);
            }
            struct pollfd pfds[2];

            pfds[0].fd = STDIN_FILENO;
            pfds[0].events = POLLIN;

            pfds[1].fd = sockfd;
            pfds[1].events = POLLIN;

            while(1){

                int timeout = -1;
                if(cmdOps.option_w == 1) timeout = cmdOps.timeout * 1000;

                int numevents = poll(pfds, 2, timeout);

                if(numevents == 0){
                    printf("Poll timed out!\n");
                    close(sockfd);
                    exit(0);
                }

                for(int i = 0; i < 2; i++) {
                    if (pfds[i].revents & POLLIN) {
                        if(pfds[i].fd == STDIN_FILENO){
                            memset(&buf, 0, sizeof buf);
                            send_message(sockfd);
                        }
                        else {
                            recv_message(sockfd);
                        }
                    }
                }
            }
        }
    }

