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
#include <hiredis/hiredis.h>
#include <event2/event.h>

// #include "LinkedList.h"
#include "Server.h"
#include "LinkedList.h"

#define CONTROLLEN CMSG_LEN(sizeof(int))
#define TIMEOUT 500



/*

    DON'T FORGET TO CONCAT THE APP NAME TO DISTINGUISH BETWEEN DIFFERENT ROOMS 

*/






typedef struct Queue Queue;

Node* rooms = NULL;
pthread_mutex_t rooms_mutex;

typedef struct Thread_args { //don't change params order
    int room_num; 
    int sockfd;
    redisContext *context;
    redisReply* reply;
    struct event_base* base;
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


void connect_to_redis(redisContext **context){
    *context = redisConnect("localhost", 6379);
    if (*context == NULL || (*context)->err) {
        if (*context) {
            printf("Connection error: %s\n", (*context)->errstr);
            redisFree(*context);
        } else {
            printf("Connection error: Can't allocate redis context\n");
        }
        return;
    }
}

void subscribe_to_redis(redisContext **context, redisReply **reply, int room){
    // Create a Redis subscriber
    *reply = redisCommand(*context, "SUBSCRIBE %d", room);
    if (*reply == NULL) {
        printf("Failed to subscribe to the channel\n");
        return;
    }
    freeReplyObject(*reply); // Free the initial subscribe reply
}

void publish_to_redis(redisContext **context, redisReply **reply, int room, char msg[]){
    // Create a Redis publisher
    *reply = redisCommand(*context, "PUBLISH %d %s", room, msg);
    if (*reply != NULL) {
        freeReplyObject(reply);
        printf("Published message to channel: %s\n", msg);
    } else {
        printf("Failed to publish the message\n");
        return;
    }
}

bool room_exists(redisContext **context, int room){
    // Check if the channel exists before subscribing
    redisReply *existsReply = redisCommand(context, "EXISTS %d", room);
    if (existsReply == NULL || existsReply->type != REDIS_REPLY_INTEGER || existsReply->integer == 0) {
        fprintf(stderr, "The channel does not exist or is empty\n");
        exit(1);
    }
    freeReplyObject(existsReply);
}

// Callback function for Redis events
void redisEventCallback(evutil_socket_t fd, short events, void* arg) {
    Thread_args* thread_args = (Thread_args*)arg;
    if (redisGetReply(thread_args->context, (void**)&thread_args->reply) != REDIS_OK) {
        printf("Failed to receive message from Redis\n");
    } else {
        if (thread_args->reply->element[0]->str && thread_args->reply->element[2]->str) {
            printf("Received message from channel %s: %s\n", thread_args->reply->element[1]->str, thread_args->reply->element[2]->str);
            int bytes_written = send(thread_args->sockfd, thread_args->reply->element[2]->str, strlen(thread_args->reply->element[2]->str), 0);
            if(bytes_written == 0){
                freeReplyObject(thread_args->reply);
                event_base_loopbreak(thread_args->base);
            }
        }
        freeReplyObject(thread_args->reply);
    }
}

// Callback function for socket events
void socketEventCallback(evutil_socket_t fd, short events, void* arg) {
    Thread_args* thread_args = (Thread_args*)arg;
    char buffer[1024] = {0};
    int bytes_read;
    bzero(buffer, sizeof(buffer));
    bytes_read = recv(fd, buffer, sizeof(buffer), 0);
    printf("checking sock %d\nbytes read:%d\n", fd, bytes_read);
    if (bytes_read > 0) {
        // Handle the socket event here
        publish_to_redis(&thread_args->context, &thread_args->reply, thread_args->room_num, buffer);
        if (thread_args->reply == NULL) {
            perror("redis publish");
            event_base_loopbreak(thread_args->base);
        }
    } else if (bytes_read == 0) {
        printf("User disconnected, exiting event loop...\n");
        event_base_loopbreak(thread_args->base);
    }
}


///////////// next implement the room loop
void* room_handler(void* arg){
    Thread_args* thread_args = (Thread_args*)arg;
    pthread_cleanup_push(room_exit_handler, &thread_args->room_num);

    connect_to_redis(&thread_args->context);
    if (thread_args->context == NULL) {
        perror("redis connect");
        pthread_exit(NULL);
    }

    subscribe_to_redis(&thread_args->context, &thread_args->reply, thread_args->room_num);
    if (thread_args->reply == NULL) {
        perror("redis subscribe");
        pthread_exit(NULL);
    }

    printf("Subscribed to the %d channel. Listening for messages...\n", thread_args->room_num);
    // Initialize libevent
    thread_args->base = event_base_new();
    // Create events for Redis and socket
    struct event* redisEvent = event_new(thread_args->base, thread_args->context->fd, EV_READ | EV_PERSIST, redisEventCallback, thread_args);
    struct event* socketEvent = event_new(thread_args->base, thread_args->sockfd, EV_READ | EV_PERSIST, socketEventCallback, thread_args);

    event_add(redisEvent, NULL);
    event_add(socketEvent, NULL);

    struct timeval timeout = {1800,0};
    event_base_loopexit(thread_args->base, &timeout);

    if (event_base_dispatch(thread_args->base) == -1) {
        perror("event_base_dispatch");
        pthread_exit(NULL);
    }

    // Cleanup and close resources as needed
    event_free(redisEvent);
    event_free(socketEvent);
    event_base_free(thread_args->base);
    redisCommand(thread_args->context, "UNSUBSCRIBE");
    redisFree(thread_args->context);
    close(thread_args->sockfd);
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
        makeSockBlocking(received_fd);

        switch (command[0]){
            case 'c':
                int room_number = rand() % 9000 + 1000;
                printf("room number: %d\n", room_number);
                Thread_args args = {.room_num=room_number, .sockfd=received_fd};
                printf("creating thread\n");
                pthread_t thread;
                pthread_create(&thread, NULL, room_handler, &args);
                pthread_mutex_lock(&rooms_mutex);
                append(&rooms, &args);
                pthread_mutex_unlock(&rooms_mutex);
                break;
            case 'j':
                printf("adding you\n");
                makeSockNonBlocking(received_fd);
                readNextArg(received_fd, room_number_str);
                makeSockBlocking(received_fd);
                /// check if such room exists in redis first
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
