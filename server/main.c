#include <stdio.h>
#include "Server.h"
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>


typedef struct thread_args
{
    struct sockaddr* sockaddr;
    int socket;
} thread_args;


void check_and_create_directory(const char *path) {
    struct stat info;
    if(!(stat(path, &info) == 0)){
        printf("directory %s doesn't exists\n", path);
        if (mkdir(path, 0777) != 0) {
            perror("Error creating directory");
            exit(1); // Exit with an error code
        }
    }

    // // printf("%s\n", full_path);
    // printf("%d\n", full_path==NULL);
    // if(!full_path!=NULL && !(stat(full_path, &info) == 0)){
    //     printf("room doesn't exists\n");
    //     if (mkdir(full_path, 0777) != 0) {
    //         perror("Error creating directory");
    //         exit(1); // Exit with an error code
    //     }
    // }
}

int get_rand() {
    return rand() % 90000 + 10000;
}

void write_state_to_file(FILE* file, char* buffer){
    int start = -1;
    int end = -1;
    int depth = 0;
    int i;

    for (i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == '{') {
            if (depth == 0) {
                start = i;
            }
            depth++;
        } else if (buffer[i] == '}') {
            depth--;
            if (depth == 0) {
                end = i;
                break;
            }
        }
    }

    // Write data to the file
    fprintf(file, "%.*s\n", end - start - 1, buffer + start + 1);
}


int create_room(char* app_name, char* buffer, char sa_data[14]){
    char path[50];
    bzero(path, 50);
    sprintf(path, "apps/%s", app_name);

    char full_path[60];
    bzero(full_path, 50);
    int room_number = get_rand();
    sprintf(full_path, "%s/%d", path, room_number);

    
    check_and_create_directory(path);
    check_and_create_directory(full_path);
    char state_path[80];
    char clients_path[80];
    sprintf(state_path, "%s/state", full_path);
    sprintf(clients_path, "%s/clients", full_path);

    FILE *state_file = fopen(state_path, "w");
    FILE *clients_file = fopen(clients_path, "wb");

    if (state_file == NULL || clients_file == NULL) {
        perror("Error opening file");
        exit(1); // Exit the program with an error code
    }
    
    fwrite(sa_data, 1, 14, clients_file);

    // for (int i = 0; i < 14; i++) {
    //     printf("%d ", sa_data[i]);
    // }
    // printf("\n");
    // printf("bytesWritten %ld\n", bytesWritten);    
        
    // Close the file when done
    fclose(clients_file);

    write_state_to_file(state_file, buffer);
    // Close the file when done
    fclose(state_file);


    // clients_file = fopen(clients_path, "rb");
    // char testing[14];
    // bzero(testing, 14);
    // size_t bytesRead = fread(testing, 1, 14, clients_file);

    // printf("printing testing %ld\n", bytesRead);
    // for (int i = 0; i < 14; i++) {
    //     printf("%d ", testing[i]);
    // }
    // printf("\n");

    // fclose(clients_file);



    return room_number;
}


void join_room(char* app_name, char* buffer, char sa_data[14], char* room_number, int socket){
    char full_path[60];
    bzero(full_path, 50);
    sprintf(full_path, "apps/%s/%s", app_name, room_number);

    printf("%s\n", full_path);

    char state_path[80];
    char clients_path[80];
    sprintf(state_path, "%s/state", full_path);
    sprintf(clients_path, "%s/clients", full_path);

    FILE *state_file = fopen(state_path, "r");
    FILE *clients_file = fopen(clients_path, "wb");

    if (state_file == NULL || clients_file == NULL) {
        perror("Error opening file");
        return; // Exit the program with an error code
    }
    
    fwrite(sa_data, 1, 14, clients_file);   
    // Close the file when done
    fclose(clients_file);



    fseek(state_file, 0, SEEK_END);  // Move the file pointer to the end of the file
    int file_size = ftell(state_file);   // Get the file size
    rewind(state_file);              // Reset the file pointer to the beginning


    char* state = (char *)malloc(file_size + 1);  // +1 for null terminator

    if (state == NULL) {
        printf("Memory allocation failed.\n");
        fclose(state_file);
        return;
    }

    // Step 4: Read the entire file into memory
    fread(state, 1, file_size, state_file);
    state[file_size] = '\0';  // Null-terminate the string

    // Close the file when done
    fclose(state_file);


    write(socket, state, file_size);
    // return state;
}



void* request_handler(void* arg){
    printf("thread started\n");
    thread_args params = *(thread_args*)arg;

    int len_read = 0; 
    char buffer[3000];
    bzero(buffer, 3000);

    read(params.socket, buffer, 3000);
    
    char app_name[50];
    bzero(app_name, 50);
    char command[50];
    bzero(command, 50);

    sscanf(buffer, "%s %s", app_name, command);

    printf("%s\n", app_name); 
    printf("%s\n", command);
    

    if(strncasecmp(command, "create", 6) == 0){
        char response[50];
        bzero(response, 50);
        int room_number = create_room(app_name, buffer, params.sockaddr->sa_data);
        sprintf(response, "room_number: %d\n", room_number);
        write(params.socket, response, strlen(response));

    }else if(strncasecmp(command, "join", 4) == 0){
        // sprintf(response, "if i were a wealth man aooooiiiii\n");
        char room_number[10];
        bzero(room_number, 10);
        sscanf(buffer + strlen(app_name) + strlen(command) + 2, "%s", room_number);
        printf("%s\n", room_number);

        //don't forget to free
        join_room(app_name, buffer, params.sockaddr->sa_data, room_number, params.socket);

    }else if(strncasecmp(command, "update", 4) == 0){
        // sprintf(response, "if i were a wealth man aooooiiiii\n");
        char room_number[10];
        bzero(room_number, 10);
        sscanf(buffer + strlen(app_name) + strlen(command) + 2, "%s", room_number);
        printf("%s\n", room_number);

        //don't forget to free
        join_room(app_name, buffer, params.sockaddr->sa_data, room_number, params.socket);

    }
    
    close(params.socket);


    return NULL;
}


void launch(struct Server* server){
    char buffer[30000];
    char* hello = "HTTP/1.1 200 OK\n"
        "Date: Sun, 01 Oct 2023 22:19:57 GMT\n"
        "Server: Apache/2.2.14 (Win32)\n"
        "Last-Modified: Wed, 27 Sep 2023 13:55:56 GMT\n"
        "Content-Length: 88\n"
        "Content-Type: text/html\n"
        "Connection: close\n\n"

        "<!DOCTYPE html>"
        "<body>"
        "<h1>Hello,World!</h1>"
        "</body>"
        "</html>";

    int address_length = sizeof(server->address);
    int count = 0;
    while(1){    
        printf("===== waiting for connection =====\n");
        
        int new_socket = accept(server->socket, (struct sockaddr*)&server->address, (socklen_t*)&address_length);

        pthread_t thread;
        thread_args args = {.sockaddr=(struct sockaddr*)&server->address, .socket=new_socket};

        if (pthread_create(&thread, NULL, request_handler, &args) != 0) {
            perror("pthread_create");
            exit(1);
        }


        // int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        // if (sockfd == -1) {
        //     perror("socket");
        //     return;
        // }


        // server->address.sin_addr.s_addr = s_addr;
        // if(connect(sockfd, (struct sockaddr *)&server->address, (socklen_t)address_length) < 0) {
        //     perror("connect");
        //     return;
        // }

        // // write(new_socket, hello, strlen(hello));
        // char request[] = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
        // if (send(sockfd, request, strlen(request), 0) == -1) {
        //     perror("send");
        //     return;
        // }
        // close(sockfd);
        
    }
}


int main(){
    srand(time(NULL));
    check_and_create_directory("apps");
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0,
        INADDR_ANY, 8080, 10, launch);
    server.launch(&server);
}