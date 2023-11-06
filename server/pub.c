#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#include "Websocket.h"


#define PORT 8080  // Port on which the server will listen


#define MAX_FRAME_SIZE 32776


int handle_frame(int sockfd, char* frame, int frame_len){
    // printf("frame: %s\n", frame);
    int opcode = get_frame_type(frame, frame_len);
    switch (opcode){
        case WS_OPCODE_PING: ///
            printf("Ping received\n");
            handle_ping(sockfd, frame);
            break;

        case WS_OPCODE_PONG:
            printf("Pong received\n");
            break;

        case WS_OPCODE_CONTINUATION: /// do nothing
        case WS_OPCODE_TEXT: /// do nothing
        case WS_OPCODE_BINARY: /// do nothing
            send(sockfd, frame, frame_len, 0);
            break;

        case WS_OPCODE_CLOSE: /// close sockfd
        default:
            return -1;
            break;
    }
    
}


typedef struct Thread_args{
    int sockfd;
}Thread_args;



void* connection_handler(void* arg){
    char buffer[MAX_FRAME_SIZE];
    char route[50] = {0};

    Thread_args* thread_args = (Thread_args*)arg;
    memset(buffer, 0, sizeof(buffer));
    recv(thread_args->sockfd, buffer, MAX_FRAME_SIZE, 0);

    // Receive data from the client and print it
    // printf("Data received: %s\n", buffer);

    sscanf(buffer, "POST %s HTTP", route);
    printf("\n\nparams: %s\n\n", route);

    if(strlen(route) == 0){
        perror("route is bad\n");
        return NULL;
    }

    int res = handle_handshake(thread_args->sockfd, buffer);
    if(res == -1){
        perror("Handshake failed");
        return NULL;
    }

    res = 0;
    int data_length = -1;
    do{
        memset(buffer, 0, sizeof(buffer));
        memset(route, 0, sizeof(route));
        data_length = recv(thread_args->sockfd, buffer, sizeof(buffer), 0);

        res = handle_frame(thread_args->sockfd, buffer, data_length);

    }while(res != -1);

    return NULL;
}


int main() {
    int server_socket, new_socket;
    struct sockaddr_in server_addr, new_addr;
    socklen_t addr_size;
    
    // Create a socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server_socket < 0) {
        perror("Error in socket creation");
        exit(1);
    }
    
    printf("Server socket created\n");
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = PORT;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Bind the socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error in binding");
        exit(1);
    }
    
    printf("Binding success\n");
    
    // Listen for incoming connections
    if (listen(server_socket, 10) == 0) {
        printf("Listening...\n");
    } else {
        printf("Error in listening");
        exit(1);
    }
    
    addr_size = sizeof(new_addr);
    
    new_socket = accept(server_socket, (struct sockaddr*)&new_addr, &addr_size);

    Thread_args* args = calloc(1, sizeof(Thread_args));
    args->sockfd = new_socket;

    pthread_t thread;
    pthread_create(&thread, NULL, connection_handler, args);

    
    pthread_join(thread, NULL);
    pthread_cancel(thread);
    free(args);
    close(new_socket);
        
    return 0;
}



