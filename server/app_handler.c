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

// #include "LinkedList.h"
#include "Server.h"
#include "Queue.h"

#define CONTROLLEN CMSG_LEN(sizeof(int))

typedef struct Queue Queue;

// Define a structure for the message
struct Message {
    long mtype;
    char mtext[100];
};

typedef struct Thread_args {
    Queue* queue;
    pthread_mutex_t mutex;
}Thread_args;



///////////// next implement the room loop
void* room_handler(void* arg){
    Thread_args* thread_args = (Thread_args*)arg;
    // int i = 0;
    // while(argv[i] != NULL)
    //     printf("%s\n", argv[i++]);

    char response[50];
    int client_socket = -1;

    // int client_socket = thread_args->socketfd;
    // Send the HTTP request
    int count = 0;
    pthread_t thread_id = pthread_self();
    char buffer[3000];
    
    int clients[10] = {0};
    int index = 0;

    while(count < 20){
        pthread_mutex_lock(&thread_args->mutex);
        if(!isEmpty(thread_args->queue)){
            client_socket = dequeue(thread_args->queue);
            clients[index++] = client_socket;
            printf("%d joined room\n", client_socket);
            bzero(buffer, 3000);
            read(client_socket, buffer, 3000);
            count = 0;
        }
        pthread_mutex_unlock(&thread_args->mutex);

        snprintf(response,
            50,
            "hello world from %lu ! count is %d"
            "\r\n",
            thread_id,
            count
        );

        for(int i=0;i<index;i++)
            send(clients[i], response, strlen(response), 0);
        // printf("%d %d\n", res, count);
        sleep(1);
        count++;
    }

    close(client_socket);
}






int main(int argc, char** argv) {
    srand(time(NULL));

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

    // Node* head = NULL;

    Thread_args* targeted_room = NULL;

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

        // char response[50];
        // snprintf(response, 50,"hello world from child count is %d\n", count);
        // int res = send(received_fd, response, 50, 0);
        // close(received_fd);

        /*
            have to implement the following
            if(room found)
                get the mutex and queue of it
                puush into queue
            
            and also reading the rest of the arguments in the clients request
        */

        if(!created){
            Thread_args args = {.queue=createQueue()};
            pthread_mutex_init(&args.mutex, NULL);
            printf("creating thread\n");
            pthread_t thread;
            pthread_create(&thread, NULL, room_handler, &args);
            created = true;
            targeted_room = &args;
            // int room_number = rand() % 9000 + 1000;
            // printf("room number: %d\n", room_number);
            // Room room = {.args=args, .room_number=room_number};
            // append(head, &room);
        }

        if(targeted_room != NULL){
            pthread_mutex_lock(&targeted_room->mutex);
            enqueue(targeted_room->queue, received_fd);
            pthread_mutex_unlock(&targeted_room->mutex);
        }
        
        count++;
    }

    close(my_un_socket);
    // Remove the message queue
    // msgctl(msgid, IPC_RMID, NULL);

    return 0;
}
