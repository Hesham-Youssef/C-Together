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
#define MAX_STATE_SIZE 1024
#define ROOM_NUMBER_LENGTH 4



/*

    DON'T FORGET TO CONCAT THE APP NAME TO DISTINGUISH BETWEEN DIFFERENT ROOMS 

*/

char* app_name;
typedef struct Queue Queue;

Node* rooms = NULL;
pthread_mutex_t rooms_mutex;

typedef struct Thread_args { //don't change params order
    int sockfd;
    int room_num; 
    redisContext *context;
    redisReply* reply;
    struct event_base* base;
    bool creator;
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
    // makeSockNonBlocking(sockfd);
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
    // makeSockBlocking(sockfd);
}


void room_exit_handler(void* arg) {
    // This function is called when the thread exits
    Thread_args* thread_args = (Thread_args*)arg;
    printf("Thread cleanup: Exiting... room %d\n", thread_args->room_num);
    pthread_mutex_lock(&rooms_mutex);
    removeElement(&rooms, thread_args->sockfd);
    printList(rooms);
    pthread_mutex_unlock(&rooms_mutex);
    event_base_free(thread_args->base);
    redisCommand(thread_args->context, "UNSUBSCRIBE");
    redisFree(thread_args->context);
    close(thread_args->sockfd);
    free(thread_args);
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
    *reply = redisCommand(*context, "SUBSCRIBE %s-%d", app_name, room);
    if (*reply == NULL) {
        printf("Failed to subscribe to the channel\n");
        return;
    }
    freeReplyObject(*reply); // Free the initial subscribe reply
}

void publish_to_redis(redisContext **context, redisReply **reply, int room, char msg[]){
    // Create a Redis publisher
    *reply = redisCommand(*context, "PUBLISH %s-%d %s", app_name, room, msg);
    if (*reply != NULL) {
        freeReplyObject(*reply);
        printf("Published message to channel: %s\n", msg);
    } else {
        printf("Failed to publish the message\n");
        return;
    }
}

void save_state_to_redis(redisContext **context, redisReply **reply, int room, int sockfd){
    // Create a Redis publisher
    char state[MAX_STATE_SIZE];
    bzero(state, MAX_STATE_SIZE);
    int bytes_read = recv(sockfd, state, MAX_STATE_SIZE, 0);
    if(bytes_read == 0){
        close(sockfd);
        *reply = NULL;
        return;
    }
    *reply = redisCommand(*context, "SET %s-%d-key %s", app_name, room, state);
    if (*reply != NULL) {
        printf("Set value: %s\nto key: %s-%d-key\n", state, app_name, room);
    } else {
        printf("Failed to publish the message\n");
        close(sockfd);
        *reply = NULL;
    }
}

void get_state_from_redis(redisContext **context, redisReply **reply, int room){
    // Fetch the value of the key
    *reply = redisCommand(*context, "GET %s-%d-key", app_name, room);
    if (*reply == NULL) {
        fprintf(stderr, "Error fetching key %s-%d\n", app_name, room);
        pthread_exit(NULL);
    }

    if ((*reply)->type == REDIS_REPLY_STRING) {
        printf("Value of key %s-%d-key: %s\n", app_name, room, (*reply)->str);
        return;
    } else if ((*reply)->type == REDIS_REPLY_NIL) {
        printf("Key %s-%d-key does not exist in Redis.\n", app_name, room);
    } else {
        fprintf(stderr, "Unexpected reply type: %d\n", (*reply)->type);
    }
    pthread_exit(NULL);
}

bool room_exists(redisContext **context, int room) {
    redisReply *existsReply = redisCommand(*context, "PUBSUB NUMSUB %s-%d", app_name, room);
    if (existsReply == NULL || existsReply->type != REDIS_REPLY_ARRAY) {
        fprintf(stderr, "Error checking channel existence\n");
        return false;
    }
    if (existsReply->elements == 2 && existsReply->element[1]->type == REDIS_REPLY_INTEGER) {
        int numSubscribers = existsReply->element[1]->integer;
        if (numSubscribers > 0) {
            printf("Channel '%d' exists and has %d subscribers.\n", room, numSubscribers);
            freeReplyObject(existsReply);
            return true;
        } else {
            printf("Channel '%d' exists but has no subscribers.\n", room);
        }
    } else {
        printf("Channel '%d' does not exist.\n", room);
    }
    freeReplyObject(existsReply);
    return false;
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
    char buffer[MAX_STATE_SIZE] = {0};
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
    pthread_cleanup_push(room_exit_handler, thread_args);

    connect_to_redis(&thread_args->context);
    if (thread_args->context == NULL) {
        perror("redis connect");
        pthread_exit(NULL);
    }

    if(thread_args->creator){
        save_state_to_redis(&thread_args->context, &thread_args->reply, thread_args->room_num, thread_args->sockfd);
        if(thread_args->reply == NULL){
            return NULL;
        }
        char message[24 + ROOM_NUMBER_LENGTH]; // Adjust the size as needed
        bzero(message, 24 + ROOM_NUMBER_LENGTH);
        snprintf(message, sizeof(message), "Your room number: %d", thread_args->room_num);
        int bytes_written = send(thread_args->sockfd, message, strlen(message), 0);
        freeReplyObject(thread_args->reply);
        if(bytes_written == 0){
            return NULL;
        }
    }else{
        get_state_from_redis(&thread_args->context, &thread_args->reply, thread_args->room_num);
        if(thread_args->reply == NULL){
            return NULL;
        }
        int bytes_written = send(thread_args->sockfd, thread_args->reply->str, strlen(thread_args->reply->str), 0);
        freeReplyObject(thread_args->reply);
        if(bytes_written == 0){
            return NULL;
        }
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
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}


void create_user_thread(int sockfd, int room, bool creator){
    Thread_args* args = (Thread_args *)malloc(sizeof(Thread_args)); //////////////don't forget to freeee
    args->room_num = room;
    args->sockfd = sockfd;
    args->creator = creator;
    printf("creating thread\n");
    pthread_t thread;
    pthread_create(&thread, NULL, room_handler, args); 

    pthread_mutex_lock(&rooms_mutex);
    append(&rooms, args);
    printList(rooms);
    pthread_mutex_unlock(&rooms_mutex);
}



int main(int argc, char** argv) {
    srand(time(NULL));
    close(atoi(argv[3]));

    pthread_mutex_init(&rooms_mutex, NULL);
    app_name = argv[1];

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

    char command[50];
    bzero(command, 50);

    char room_number_str[10];
    bzero(room_number_str, 10);

    redisContext *context;
    connect_to_redis(&context);
    int room_number;
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
                room_number = rand() % 9000 + 1000;
                printf("room number: %d\n", room_number);
                create_user_thread(received_fd, room_number, true);
                break;
            case 'j':
                readNextArg(received_fd, room_number_str);
                room_number = atoi(room_number_str);
                if(room_exists(&context, room_number)){
                    printf("adding you\n");
                    create_user_thread(received_fd, room_number, false);
                }else{
                    if (send(received_fd, "Room doesn't exist :(", 22, 0) == -1) {
                        perror("send");
                    }
                    close(received_fd);
                }
                /// check if such room exists in redis first
                break;
            default:
                if (send(received_fd, "Not a valid command (either c to create or j to join) >:(", 22, 0) == -1) {
                    perror("send");
                    return 1;
                }
                close(received_fd);
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
