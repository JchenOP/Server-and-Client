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
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>


char buf[1024];

int alarmed = 1;
unsigned int timeout = 0;

sem_t sem;
int clients[5];
int fd_count = 0;

void signal_handler(){
    alarmed = 0;
}
void delete_client(int sockfd){
    for(int i = 0; i < fd_count; i++){
        if(clients[i] == sockfd){
            if(i == fd_count -1) clients[i] = 0;
            else {
                for (int j = i; j < fd_count; j++) {
                    clients[j] = clients[j + 1];
                }
                clients[fd_count-2] = 0;
            }
            fd_count--;
            return;
        }
    }
}
void *Client_send(void* sockfd){
    int id = *(int *) sockfd;
    while(alarmed) {
        int length;
        memset(&buf,0,sizeof buf);
        if ((length = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
            alarm(timeout);
            buf[length] = '\0';
            send(id, buf, strlen(buf), 0);
            alarm(timeout);
        } else {
            close(id);
            break;
        }
    }
}

void *Client_receive(void * sockfd){
    int id = *(int *) sockfd;
    while(alarmed) {
        memset(&buf,0,sizeof buf);
        if(recv(id, &buf, sizeof buf, 0) > 0){
            alarm(timeout);
        }
        else{
            close(id);
            break;
        }
        printf("%s", buf);
    }
}

void *Server_send(){
    while(1) {
        int length;
        memset(&buf,0,sizeof buf);
        length = read(STDIN_FILENO, buf, sizeof buf);
        buf[length] = '\0';
        for(int i = 0; i<fd_count; i++) {
            send(clients[i], buf, strlen(buf), 0);
        }
    }
}

void *Server_receive(void * sockfd){
    sem_wait(&sem);
    int id = *(int *) sockfd;
    while(1) {
        memset(&buf,0,sizeof buf);
        if(recv(id, &buf, sizeof buf, 0) > 0){
            printf("%s", buf);
            for(int i = 0; i<fd_count; i++) {
                if(clients[i] != id){
                    send(clients[i], buf, strlen(buf), 0);
                }
            }
        }
        else{
            fprintf(stderr,"disconnect");
            delete_client(id);
            close(id);
            break;
        }
    }
    sem_post(&sem);
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
    int yes = 1;

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


        if(cmdOps.option_r) sem_init(&sem, 0, 5);
        else sem_init(&sem,0,1);

        pthread_t thread_send;
        if (pthread_create(&thread_send, NULL, Server_send, NULL) != 0) {
            perror("Unable to send");
            close(sockfd);
            exit(1);
        }

        pthread_t thread_receive[20];
        int thread_id = 0;

      while(1) {
          struct sockaddr_in clientAddress;
          socklen_t size = sizeof(clientAddress);
          int clientfd;
          if ((clientfd = accept(sockfd, (struct sockaddr *) &clientAddress, &size)) == -1) {
              perror("server: fail connect");
              close(sockfd);
          } else {
              char dst[INET_ADDRSTRLEN];
              inet_ntop(clientAddress.sin_family, &clientAddress.sin_addr, dst, sizeof dst);

              printf("server: new connection from %s:%d.\n", dst, ntohs(clientAddress.sin_port));
              clients[fd_count] = clientfd;
          }
          if (pthread_create(&thread_receive[thread_id], NULL, Server_receive, &clients[fd_count]) != 0) {
              perror("Unable to receive");
              close(sockfd);
              exit(1);
          }
          fd_count++;
          thread_id++;
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

        pthread_t thread_send;
        pthread_t thread_receive;

        if(cmdOps.option_w == 1) timeout = cmdOps.timeout;

        signal(SIGALRM,signal_handler);
        alarm(timeout);

        if (pthread_create(&thread_send, NULL, Client_send, &sockfd) != 0) {
            perror("Unable to send");
            close(sockfd);
            exit(1);
        }
        if (pthread_create(&thread_receive, NULL, Client_receive, &sockfd) != 0) {
            perror("Unable to receive");
            close(sockfd);
            exit(1);
        }

        pthread_join(thread_send, NULL);
        pthread_join(thread_receive, NULL);

        close(sockfd);
        exit(0);

    }
}
