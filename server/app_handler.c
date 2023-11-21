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
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <event2/event.h>

// #include "LinkedList.h"
#include "Server.h"
#include "LinkedList.h"
#include "Websocket.h" 


#define bzero(s, n) memset((s), 0, (n))
#define bcopy(s1, s2, n) memmove((s2), (s1), (n))

#define CONTROLLEN CMSG_LEN(sizeof(int))
#define TIMEOUT 500
#define MAX_STATE_SIZE 1024
#define ROOM_NUMBER_LENGTH 4
#define ID_LENGTH 4


#define CREATOR 0
#define CLIENT 1
#define CONTROL 2


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
    redisContext *sub_context;
    redisContext *pub_context;
    redisReply* reply;
    struct event_base* base;
    short type;
    int id;

    redisAsyncContext *ac;
    int current_serving;
}Thread_args;

// void makeSockNonBlocking(int sockfd){
//     int flags = fcntl(sockfd, F_GETFL, 0);
//     if (flags == -1) {
//         perror("fcntl");
//         exit(1);
//     }
//     if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
//         perror("fcntl");
//         exit(1);
//     }
// }

// void makeSockBlocking(int sockfd){
//     int flags = fcntl(sockfd, F_GETFL, 0);
//     if (flags == -1) {
//         perror("fcntl");
//         exit(1);
//     }
//     flags ^= O_NONBLOCK;
//     if (fcntl(sockfd, F_SETFL, flags | 0) == -1) {
//         perror("fcntl");
//         exit(1);
//     }
// }


// void readNextArg(int sockfd, char buff[]){
//     // makeSockNonBlocking(sockfd);
//     int offset = 0, bytes_read;
//     while((offset < 50 && bytes_read != -1)){
//         bytes_read = recv(sockfd, buff+offset, 1, 0);
//         // printf("%c %d %d\n", buffer[offset], buffer[offset], offset);
//         if(buff[offset] == 32){
//             buff[offset] = 0;
//             break;
//         }
//         offset++;
//     }
//     printf("done reading %s\n", buff);
//     // makeSockBlocking(sockfd);
// }


void room_exit_handler(void* arg) {
    // This function is called when the thread exits
    Thread_args* thread_args = (Thread_args*)arg;
    printf("Thread cleanup: Exiting... room %d\n", thread_args->room_num);
    pthread_mutex_lock(&rooms_mutex);
    removeElement(&rooms, thread_args->sockfd);
    printList(rooms);
    pthread_mutex_unlock(&rooms_mutex);
    event_base_free(thread_args->base);
    redisCommand(thread_args->sub_context, "UNSUBSCRIBE");
    redisFree(thread_args->sub_context);
    redisFree(thread_args->pub_context);
    close(thread_args->sockfd);
    free(thread_args);

    if(rooms == NULL)
        raise(SIGTERM);
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

void subscribe_to_redis(Thread_args* args, short type){
    switch (type){
        case CONTROL:
            args->reply = redisCommand(args->sub_context, "SUBSCRIBE %s-%d-control", app_name, args->room_num);
            break;
        default:
            args->reply = redisCommand(args->sub_context, "SUBSCRIBE %s-%d", app_name, args->room_num);
            break;
    }
    
    if (args->reply == NULL) {
        printf("Failed to subscribe to the channel\n");
        return;
    }
    freeReplyObject(args->reply); // Free the initial subscribe reply
}

void publish_to_redis(Thread_args* args, char msg[], short type){
    // Create a Redis publisher

    switch (type){
        case CONTROL:
            args->reply = redisCommand(args->pub_context, "PUBLISH %s-%d-control \"%d-%s\"", app_name, args->room_num, args->id, msg);
            break;
        default:
            args->reply = redisCommand(args->pub_context, "PUBLISH %s-%d \"%d-%s\"", app_name, args->room_num, args->id, msg);
            break;
    }

    if (args->reply != NULL) {
        printf("Published message to channel: %s\n", args->reply->str);
        freeReplyObject(args->reply);
    } else {
        printf("Failed to publish the message\n");
        return;
    }
}




bool check_room(redisContext **context, int room, short type) {
    redisReply *existsReply;
    switch (type){
        case CONTROL:
            existsReply = redisCommand(*context, "PUBSUB NUMSUB %s-%d-control", app_name, room);
            break;
        default:
            existsReply = redisCommand(*context, "PUBSUB NUMSUB %s-%d", app_name, room);
            break;
    }
    if (existsReply == NULL || existsReply->type != REDIS_REPLY_ARRAY) {
        perror("Checking channel existence");
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

void contact_control(Thread_args* args, char* msg){
    printf("hel\n");
    
    
    args->reply = redisCommand(args->pub_context, "PUBLISH %s-%d-control \"%d-%s\"", app_name, args->room_num, args->id, msg);
    printf("www\n");
    if (args->reply != NULL) {
        printf("Published message to channel: %s\n", args->reply->str);
        freeReplyObject(args->reply);
    } else {
        printf("Failed to publish the message\n");
        return;
    }
}

// Callback function for handling BLPOP result
// (redisAsyncContext *c, void *reply, void *privdata)
void blpopCallback(redisAsyncContext *ac, void *arg, void *privdata) {
    redisReply *reply = (redisReply *)arg;
    Thread_args* thread_args = (Thread_args*)privdata;

    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        printf("BLPOP error: %s\n", reply ? reply->str : "Unknown error");
    } else if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
        printf("Popped from queue: %s\n", reply->element[1]->str);

        //// now after poping the request what should be done is set the current serving to the id
        //// then send the message associated witht the request to the control (no need right now, cuz we assume all is just get state)
        //// then for all messages the upcoming messages the control sends we will just append the id of current serving to it
        //// until a fin frame is recevied and that frame has a type associated with data
        //// then remove the current serving and use the line below to go back waiting
    

        // redisAsyncCommand(thread_args->ac, blpopCallback, thread_args, "BLPOP admin_client_queue 0");

        // Process the admin-client pair, update admin state, etc.
    } else {
        printf("Unexpected BLPOP reply format\n");
    }
}



// Callback function for Redis events
void redisEventCallback(evutil_socket_t fd, short events, void* arg) {
    Thread_args* thread_args = (Thread_args*)arg;
    if (redisGetReply(thread_args->sub_context, (void**)&thread_args->reply) != REDIS_OK) {
        printf("Failed to receive message from Redis\n");
    } else {
        if (thread_args->reply->element[0]->str && thread_args->reply->element[2]->str) {
            printf("Received message from channel %s: %s\n", thread_args->reply->element[1]->str, thread_args->reply->element[2]->str);
            *((thread_args->reply->element[2]->str)+ID_LENGTH+1) = '\0';
            printf("id: %d\n", atoi((thread_args->reply->element[2]->str)+1));
            if(thread_args->id != atoi((thread_args->reply->element[2]->str)+1)){
                int bytes_written = send(thread_args->sockfd, thread_args->reply->element[2]->str+ID_LENGTH+2, strlen(thread_args->reply->element[2]->str+ID_LENGTH+2)-1, 0);
                if(bytes_written == 0){
                    freeReplyObject(thread_args->reply);
                    event_base_loopbreak(thread_args->base);
                }
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
        publish_to_redis(thread_args, buffer, thread_args->type);
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

    connect_to_redis(&thread_args->sub_context);
    if (thread_args->sub_context == NULL) {
        perror("redis connect");
        pthread_exit(NULL);
    }

    connect_to_redis(&thread_args->pub_context);
    if (thread_args->pub_context == NULL) {
        perror("redis connect");
        pthread_exit(NULL);
    }


    if(thread_args->type == CREATOR){
        // save_state_to_redis(thread_args);
        // if(thread_args->reply == NULL){
        //     return NULL;
        // }
        char message[24 + ROOM_NUMBER_LENGTH]; // Adjust the size as needed
        bzero(message, 24 + ROOM_NUMBER_LENGTH);
        snprintf(message, sizeof(message), "Your room number: %d", thread_args->room_num);
        // int bytes_written = send(thread_args->sockfd, message, strlen(message), 0);
        // freeReplyObject(thread_args->reply);
        if(send_websocket_message(thread_args->sockfd, message, strlen(message)) == 0){
            return NULL;
        }
    }else{
        // get_state_from_redis(thread_args);
        // if(thread_args->reply == NULL){
        //     return NULL;
        // }
        // freeReplyObject(thread_args->reply);

        contact_control(thread_args, "A new user joined");
        if(send_websocket_message(thread_args->sockfd, "Joined room successfully", 25) <= 0){
            return NULL;
        }
    }

    subscribe_to_redis(thread_args, thread_args->type);
    if (thread_args->reply == NULL) {
        perror("redis subscribe");
        pthread_exit(NULL);
    }

    printf("Subscribed to the %d channel. Listening for messages...\n", thread_args->room_num);
    // Initialize libevent
    thread_args->base = event_base_new();
    // Create events for Redis and socket
    thread_args->ac = redisAsyncConnect("localhost", 6379);
    if (thread_args->ac->err) {
        printf("Error: %s\n", thread_args->ac->errstr);
        exit(EXIT_FAILURE);
    }

    // Attach the Redis event to the event base
    

    // Set BLPOP callback
    redisAsyncCommand(thread_args->ac, blpopCallback, thread_args, "BLPOP admin_client_queue 0");
    struct event* redisEvent = event_new(thread_args->base, thread_args->sub_context->fd, EV_READ | EV_PERSIST, redisEventCallback, thread_args);
    struct event* socketEvent = event_new(thread_args->base, thread_args->sockfd, EV_READ | EV_PERSIST, socketEventCallback, thread_args);

    redisLibeventAttach(thread_args->ac, thread_args->base);
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
    redisAsyncFree(thread_args->ac);
    event_free(socketEvent);
    pthread_cleanup_pop(1);
    // close(thread_args->sockfd);
    pthread_exit(NULL);
}


void create_user_thread(int sockfd, int room, short type){
    Thread_args* args = (Thread_args *)malloc(sizeof(Thread_args)); //////////////don't forget to freeee
    args->room_num = room;
    args->sockfd = sockfd;
    args->type = type;
    args->id = rand() % 9000 + 1000;
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
    char params[32];
    char fd_buf[CONTROLLEN];
    memset(fd_buf, 0, CONTROLLEN);

    struct cmsghdr *cmptr;

    iov.iov_base = &params;
    iov.iov_len = 32;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_control = fd_buf;
    msg.msg_controllen = CONTROLLEN;

    bool created = false;

    char buffer[1024] = {0};


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
        printf("read params is %s\n", params);

        memset(buffer, 0, 1024);
        recv(received_fd, buffer, 1024, 0);
        if(handle_handshake(received_fd, buffer) > 0){
            perror("handshake failed");
            close(received_fd);
            continue;
        }
        
        // sleep(5);
        // ws_send_ping(received_fd, "send state please\n", 19);
        // sleep(5);
        // close(received_fd);
        char* rest = params;
        char* token = strtok_r(rest, "/", &rest);
        switch (token[0]){
            case 'c':
                room_number = rand() % 9000 + 1000;
                printf("room number c: %d\n", room_number);
                create_user_thread(received_fd, room_number, CREATOR);
                break;
            case 'j':
                // readNextArg(received_fd, room_number_str);
                if((token = strtok_r(rest, "/", &rest)) == NULL){
                    perror("Incomplete paramaters");
                    close(received_fd);
                }
                
                room_number = atoi(token);
                printf("room number j: %d\n", room_number);
                if(check_room(&context, room_number, CLIENT)){
                    printf("adding you\n");
                    token = strtok_r(rest, "/", &rest);
                    create_user_thread(received_fd, room_number, (token == NULL)?(CLIENT):((token[0] == 'c')?CONTROL:CLIENT));
                }else{
                    if (send_websocket_message(received_fd, "Room doesn't exist :(", 22) == -1) {
                        perror("send");
                    }
                    close(received_fd);
                }
                /// check if such room exists in redis first
                break;
            default:
                if (send_websocket_message(received_fd, "Not a valid command (either c to create or j to join) >:(", 22) == -1) {
                    perror("send");
                    return 1;
                }
                close(received_fd);
                break;
        }

    }while(rooms != NULL);

    close(my_un_socket);
    pthread_mutex_destroy(&rooms_mutex);
    printf("exiting app handler %s\n", argv[1]);
    printList(rooms);

    return 0;
}
