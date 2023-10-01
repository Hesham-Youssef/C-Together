#include <stdio.h>
#include "Server.h"
#include <string.h>
#include <unistd.h>


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
    while(1){    
        printf("===== waiting for connection =====\n");
        int new_socket = accept(server->socket, (struct sockaddr*)&server->address, (socklen_t*) &address_length);

        read(new_socket, buffer, 30000);
        printf("%s\n", buffer);
        write(new_socket, hello, strlen(hello));
        close(new_socket);
    }
}


int main(){
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0,
        INADDR_ANY, 8080, 10, launch);
    server.launch(&server);
}