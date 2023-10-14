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

#define CONTROLLEN CMSG_LEN(sizeof(int))

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

    char response[50];

    int client_socket = thread_args->socketfd;
    // Send the HTTP request
    int count = 0;
    while(count < 10){
        snprintf(response,
            50,
            "hello world from child! count is %d"
            "\r\n",
            count
        );
        int res = send(client_socket, response, strlen(response), 0);
        printf("%d %d\n", res, count);
        sleep(1);
        count++;
    }

    close(client_socket);
}






int main(int argc, char** argv) {
    printf("hello world inside new child\n");
    int my_un_socket = atoi(argv[2]);
    int received_fd;
    struct msghdr msg;
    struct iovec iov;
    char dummy;
    char fd_buf[CONTROLLEN];
    struct cmsghdr *cmptr;

    iov.iov_base = &dummy;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_control = fd_buf;
    msg.msg_controllen = CONTROLLEN;

    int count = 0;

    while(1){
        if (recvmsg(my_un_socket, &msg, 0) <= 0) {
            perror("recvmsg");
            exit(1);
        }

        cmptr = CMSG_FIRSTHDR(&msg);
        if (cmptr == NULL || cmptr->cmsg_len != CONTROLLEN) {
            perror("bad control message");
            exit(1);
        }

        received_fd = *((int *)CMSG_DATA(cmptr));
        printf("Child process received file descriptor: %d\n", received_fd);

        char response[50];
        snprintf(response, 50,"hello world from child count is %d\n", count);
        // int res = send(received_fd, response, 50, 0);
        // close(received_fd);

        Thread_args args = {.socketfd=received_fd};
        pthread_t thread;
        pthread_create(&thread, NULL, room_handler, &args);
        count++;
    }

    close(my_un_socket);
    // Remove the message queue
    // msgctl(msgid, IPC_RMID, NULL);

    return 0;
}
