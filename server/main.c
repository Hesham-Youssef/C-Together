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

// Define a structure to store name-PID mappings
struct ProcessInfo {
    pid_t pid;
    char name[32];
};

struct Message {
    long mtype;
    char mtext[100];
};


void send_msg_to_app(char* app_name, char* msg, size_t msg_len){
    key_t key;
    int msgid;
    struct Message message;

    key = ftok(app_name, 'A');
    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }
    //a bit dangerous because 
    message.mtype = 1;
    bzero(message.mtext, 100);
    memcpy(message.mtext, msg, msg_len);

    for(int i=0;i<16;i++)
        printf("%d=", message.mtext[i]);
    printf("\n");
    // Send the message to the queue
    if (msgsnd(msgid, &message, msg_len, 0) == -1) {
        perror("msgsnd");
        exit(1);
    }


    // Remove the message queue
    // msgctl(msgid, IPC_RMID, NULL);
}




void create_app_handler(char* app_name){
    pid_t child_pid;
    child_pid = fork();
    if (child_pid == -1) {
        perror("Fork failed");
        return;
    }

    if (child_pid == 0) {
        // strcpy(processes[0].name, app_name);
        // processes[0].pid = getpid();
        char num_str[10];
        bzero(num_str, 10);
        // snprintf(num_str, 10, "%d", socketfd);
        char *args[] = {"./app_handler", app_name, NULL}; 
        if (execvp("./app_handler", args) == -1) {
            perror("Exec failed");
            exit(1);
        }
    } else {
        // close(socketfd);
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

    struct sockaddr_un addr;
    bool process_created = false;
    char msg[100];

    // // Prepare the socket for passing
    // struct msghdr msg = {0};
    // struct iovec iov[1];
    // char buf[1];
    // iov[0].iov_base = buf;
    // iov[0].iov_len = 1;

    // struct cmsghdr *cmsg;
    // char control[CMSG_SPACE(sizeof(int))];
    // msg.msg_iov = iov;
    // msg.msg_iovlen = 1;
    // msg.msg_control = control;
    // msg.msg_controllen = sizeof(control);

    // cmsg = CMSG_FIRSTHDR(&msg);
    // cmsg->cmsg_level = SOL_SOCKET;
    // cmsg->cmsg_type = SCM_RIGHTS;
    // cmsg->cmsg_len = CMSG_LEN(sizeof(int));


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
        bzero(msg, 16);
        

        for(int i=0;i<16;i++)
            printf("%d ", (((struct sockaddr*)&server->address)->sa_data)[i]);
        printf("\n");

        memcpy(msg, ((struct sockaddr*)&server->address)->sa_data, 16);

        send_msg_to_app(app_name, msg, 16);

        if(!process_created){
            create_app_handler(app_name);
            process_created = true;
        }

        // close(new_socket);
    }
}


int main(){
    srand(time(NULL));
    // send_msg_to_app("connect4", "resputian\n");
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0,
        INADDR_ANY, 8080, 10, launch);
    server.launch(&server);
    return 0;
}