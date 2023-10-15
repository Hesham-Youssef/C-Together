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
#include <sys/msg.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>

#define CONTROLLEN CMSG_LEN(sizeof(int))

// Define a structure to store name-PID mappings
struct ProcessInfo {
    pid_t pid;
    char name[32];
};

struct Message {
    long mtype;
    char mtext[100];
};


void send_msg_to_app(int socketfd, struct msghdr* msg){
    // key_t key;
    // int msgid;
    // struct Message message;

    // key = ftok(app_name, 'A');
    // msgid = msgget(key, 0666 | IPC_CREAT);
    // if (msgid == -1) {
    //     perror("msgget");
    //     exit(1);
    // }
    // //a bit dangerous because 
    // message.mtype = 1;
    // bzero(message.mtext, 100);
    // memcpy(message.mtext, msg, msg_len);

    // // Send the message to the queue
    // if (msgsnd(msgid, &message, msg_len, 0) == -1) {
    //     perror("msgsnd");
    //     exit(1);
    // }
    printf("hello world inside send msg\n");

    if (sendmsg(socketfd, msg, MSG_NOSIGNAL) != 1) {
        perror("sendmsg");
        exit(1);
    }

    // Remove the message queue
    // msgctl(msgid, IPC_RMID, NULL);
}




int create_app_handler(char* app_name){
    printf("hello world inside create\n");
    int sockfd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) < 0) {
        perror("socketpair");
        exit(1);
    }

    pid_t child_pid;
    child_pid = fork();
    if (child_pid == -1) {
        perror("Fork failed");
        exit(1);
    }

    if (child_pid == 0) {
        // strcpy(processes[0].name, app_name);
        // processes[0].pid = getpid();
        close(sockfd[0]);

        char sockfd_str[10];
        bzero(sockfd_str, 10);
        snprintf(sockfd_str, 10, "%d", sockfd[1]);
        char *args[] = {"./app_handler", app_name, sockfd_str, NULL}; 
        if (execvp("./app_handler", args) == -1) {
            perror("Exec failed");
            exit(1);
        }
    } else {
        close(sockfd[1]);

        return sockfd[0];
        //// handle child exit signal
        // int status;
        // wait(&status);
        // printf("Parent Process: Child exited with status %d\n", WEXITSTATUS(status));
    }
}



void launch(struct Server* server){
    struct ProcessInfo processes[100];
    char buffer[3000];
    int address_length = sizeof(server->address);
    int count = 0;

    // struct sockaddr_un addr;
    bool process_created = false;
    // char app_msg[100];


    struct msghdr msg;
    struct iovec iov;
    char dummy = 'D'; ////use this to send to app_handler whether we join or create
    char control[CONTROLLEN];
    struct cmsghdr *cmptr;

    iov.iov_base = &dummy;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CONTROLLEN;
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;

    int un_socket = -1;

    while(1){    
        printf("===== waiting for connection =====\n");
        bzero(buffer, 3000);
        int new_socket = accept(server->socket, (struct sockaddr*)&server->address, (socklen_t*)&address_length);
        read(new_socket, buffer, sizeof(buffer));
        printf("%s\n", buffer);

        char app_name[32];
        bzero(app_name, 32);
        sscanf(buffer, "%s ", app_name);
        

        // pid_t target_pid = -1;
        // for (int i = 0; i < 10; i++) {
        //     if (strcmp(processes[i].name, app_name) == 0) {
        //         target_pid = processes[i].pid;
        //         break;
        //     }
        // }
        // bzero(app_msg, 16);
        // memcpy(app_msg, ((struct sockaddr*)&server->address)->sa_data, 16);

        if(!process_created){
            un_socket = create_app_handler(app_name);
            process_created = true;
        }

        *((int *)CMSG_DATA(cmptr)) = new_socket;
        send_msg_to_app(un_socket, &msg);

        printf("hello world after send msg\n");

        // close(new_socket);
    }
}


int main(){
    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL));
    // send_msg_to_app("connect4", "resputian\n");
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0,
        INADDR_ANY, 8080, 10, launch);
    server.launch(&server);
    return 0;
}