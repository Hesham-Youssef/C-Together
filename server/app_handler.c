#include <stdio.h>
#include "Server.h"
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/socket.h>
#include <sys/un.h>

// Define a structure for the message
struct Message {
    long mtype;
    char mtext[100];
};

typedef struct Thread_args {
    int socketfd;
}Thread_args;


void* room_handler(void* arg){
    Thread_args* thread_args = (Thread_args*)arg;
    // int i = 0;
    // while(argv[i] != NULL)
    //     printf("%s\n", argv[i++]);

    char *response = "hello world from child!"
                    "\r\n";

    int client_socket = thread_args->socketfd;
    // Send the HTTP request
    int count = 0;
    while(count < 60){
        int res = send(client_socket, response, strlen(response), 0);
        printf("%d\n", res);
        sleep(1);
        count++;
    }

    close(client_socket);
}






int main(int argc, char** argv) {
    key_t key;
    int msgid;
    struct Message message;

    // Generate the same key as the producer
    key = ftok(argv[1], 'A');

    // Access the existing message queue
    msgid = msgget(key, 0666);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }

    while(1){
        // Receive a message from the message queue
        bzero(message.mtext, 100);
        if (msgrcv(msgid, &message, sizeof(message.mtext), 0, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }
        
        for(int i=0;i<16;i++)
            printf("%d ", message.mtext[i]);
        printf("\n");

        // pthread_t thread;
        Thread_args thread_args;

        int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd == -1) {
            perror("Socket creation failed");
            exit(1);
        }

        struct sockaddr addr = {.sa_family=AF_UNIX};
        memcpy(addr.sa_data, message.mtext, 16);

        if (connect(sockfd, &addr, sizeof(addr)) == -1) {
            perror("Connection failed");
            close(sockfd);
            exit(1);
        }

        char msg[23] = "hello world from child\n";
        int res = send(sockfd, msg, 23, 0);
        close(sockfd);
        // pthread_create(&thread, NULL, room_handler, &thread_args);
    }
    // Remove the message queue
    msgctl(msgid, IPC_RMID, NULL);

    return 0;
}
