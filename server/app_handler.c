#include <stdio.h>
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
#include <stdbool.h>
#include <fcntl.h>
#include <poll.h>

// #include "LinkedList.h"
#include "Server.h"
#include "LinkedList.h"

#define CONTROLLEN CMSG_LEN(sizeof(int))
#define TIMEOUT 500

typedef struct Queue Queue;

Node* rooms = NULL;
pthread_mutex_t rooms_mutex;

// Define a structure for the message
struct Message {
    long mtype;
    char mtext[100];
};

typedef struct Thread_args { //don't change params order
    int room_num;
    Node* queue;
    pthread_mutex_t mutex;
}Thread_args;

void makeSockNonBlocking(int sockfd){
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        exit(1);
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(1);
    }
}

void makeSockBlocking(int sockfd){
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        exit(1);
    }
    flags ^= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags | 0) == -1) {
        perror("fcntl");
        exit(1);
    }
}


void readNextArg(int sockfd, char buff[]){
    int offset = 0, bytes_read;
    while((offset < 50 && bytes_read != -1)){
        bytes_read = recv(sockfd, buff+offset, 1, 0);
        // printf("%c %d %d\n", buffer[offset], buffer[offset], offset);
        if(buff[offset] == 32){
            buff[offset] = 0;
            break;
        }
        offset++;
    }
    printf("done reading %s\n", buff);
}


void room_exit_handler(void* arg) {
    // This function is called when the thread exits
    printf("Thread cleanup: Exiting... room %d\n", *((int*)arg));
    pthread_mutex_lock(&rooms_mutex);
    removeElement(&rooms, *((int*)arg));
    printList(rooms);
    pthread_mutex_unlock(&rooms_mutex);
}


void forward_to_clients(Node* clients, char* buff){
    Node* curr = clients;
    Node* next = NULL;
    int client_socket;
    while(curr != NULL){
        client_socket = (int)((unsigned long)(curr->data));
        int bytes_written = send(client_socket, buff, strlen(buff), 0);
        if(bytes_written == -1){
            printf("deleting %d\n", client_socket);
            next = curr->next;
            removeNode(&clients, curr);
            close(client_socket);
            curr = next;
            continue;
        }
        curr = curr->next;
    }
}



///////////// next implement the room loop
void* room_handler(void* arg){
    Thread_args* thread_args = (Thread_args*)arg;

    pthread_cleanup_push(room_exit_handler, &thread_args->room_num);

    char response[50];
    int client_socket = -1;

    int room_mode = 1;

    int count = 0;
    pthread_t thread_id = pthread_self();
    char buffer[5000];
    
    Node* clients = NULL;
    Node* curr = NULL;
    Node* next = NULL;
    Node* toCurr = NULL;

    unsigned long temp = 0;

    int bytes_read;

    do{
        while(1){
            pthread_mutex_lock(&thread_args->mutex);
            if(is_empty(thread_args->queue)){
                pthread_mutex_unlock(&thread_args->mutex);
                break;
            }
            client_socket = *((int*)(pop(&thread_args->queue)));
            pthread_mutex_unlock(&thread_args->mutex);

            printf("hello world from room %lu \n", thread_id);

            temp = client_socket;
            append(&clients, (void*)temp);
            count = 0;
        }

        snprintf(response,
            50,
            "hello world from %lu ! count is %d"
            "\r\n",
            thread_id,
            count
        );
        
        curr = clients;
        
        while(curr != NULL){
            client_socket = (int)((unsigned long)(curr->data));
            bzero(buffer, sizeof(buffer));
            bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
            printf("checking sock %d\nbytes read:%d\n", client_socket, bytes_read);
            if(bytes_read > 0){
                forward_to_clients(clients, buffer);
            }
            curr = curr->next;
        }
        
        sleep(1);
        count++;
    }while(clients != NULL);

    curr = clients;
    while(curr != NULL){
        close((int)((unsigned long)(curr->data)));
        next = curr->next;
        removeNode(&clients, curr);
        curr = next;
    }

    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}






int main(int argc, char** argv) {
    srand(time(NULL));
    close(atoi(argv[3]));

    pthread_mutex_init(&rooms_mutex, NULL);

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
    bool created = false;

    
    // Thread_args* targeted_room = NULL;

    char command[50];
    bzero(command, 50);

    char room_number_str[10];
    bzero(room_number_str, 10);

    int offset = 0;
    do{
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

        makeSockNonBlocking(received_fd);
        readNextArg(received_fd, command);

        

        switch (command[0]){
            case 'c':
                Thread_args args = {.queue=NULL};
                append(&args.queue, &received_fd);
                pthread_mutex_init(&args.mutex, NULL);
                printf("creating thread\n");
                pthread_t thread;
                pthread_create(&thread, NULL, room_handler, &args);
                // created = true;
                // targeted_room = &args;
                int room_number = rand() % 9000 + 1000;
                printf("room number: %d\n", room_number);
                args.room_num = room_number;
                pthread_mutex_lock(&rooms_mutex);
                append(&rooms, &args);
                pthread_mutex_unlock(&rooms_mutex);
                break;
            case 'j':
                printf("adding you\n");
                readNextArg(received_fd, room_number_str);
                pthread_mutex_lock(&rooms_mutex);
                Node* targeted_room = search(rooms, atoi(room_number_str));
                pthread_mutex_unlock(&rooms_mutex);
                if(targeted_room == NULL){
                    printf("Room not found\n");
                    send(received_fd, "Room not found\n", 16, 0);
                    close(received_fd);
                    break;
                }
                pthread_mutex_lock(&((Thread_args*)(targeted_room->data))->mutex);
                append(&((Thread_args*)(targeted_room->data))->queue, &received_fd);
                pthread_mutex_unlock(&((Thread_args*)(targeted_room->data))->mutex);
            
        
                break;
            default:
                break;
        }

        count++;
    }while(rooms != NULL);

    close(my_un_socket);
    // Remove the message queue
    // msgctl(msgid, IPC_RMID, NULL);

    printf("exiting app handler %s\n", argv[1]);

    return 0;
}
