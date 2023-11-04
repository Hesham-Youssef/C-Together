#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>


#define PORT 8080  // Port on which the server will listen
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define GUID_LEN 37
#define HASH_LEN 20

// key need to be on the form of "key:"
// case sensitive
// when value is NULL returns a copy of the value
// note the copy freeing is up to you
char* check_key_has_value(char* msg, char* key, char* value){
    int i=0;
    char* val;
    char* keyptr = strstr(msg, key);
    if(keyptr == NULL)
        return NULL;

    while(keyptr[i++] != '\r');

    keyptr[i-1] = '\0';
    val = strstr(keyptr, value);
    keyptr[i-1] = '\r';

    return val;
}

int get_key_value(char* msg, char* key, char** value_return){
    int i=0;
    char* keyptr = strstr(msg, key);
    if(keyptr == NULL)
        return -1;

    while(keyptr[i++] != '\r');
    keyptr[i-1] = '\0';

    int key_len = strlen(key);
    int value_len = i - key_len - 1;
    *value_return = calloc(value_len, sizeof(char));
    memcpy(*value_return, keyptr+key_len+1, value_len);
    
    keyptr[i-1] = '\r';

    return value_len;
}

char* Base64Encode(const unsigned char *input, int length){
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char *buff = (char *)calloc(bptr->length, sizeof(char));
    memcpy(buff, bptr->data, bptr->length-1);
    buff[bptr->length-1] = 0;
    BIO_free_all(b64);

    return buff;
}


char* process_handshake_msg(char* msg){
    char* res;
    res = check_key_has_value(msg, "Connection:", "Upgrade");
    if(res == NULL){
        printf("connection upgrade doesn't exist\n");
        return NULL;
    }
    res = check_key_has_value(msg, "Upgrade:", "websocket");
    if(res == NULL){
        printf("upgrade websocket doesn't exist\n");
        return NULL;
    }
    
    return res;
}


int handle_handshake(int sockfd, char* msg){
    char* res = process_handshake_msg(msg);
    if(res == NULL){
        printf("invalid request\n");
        return -1;
    }
    int value_len = get_key_value(msg, "Sec-WebSocket-Key:", &res); //needs to be freed
    if(value_len == -1){
        printf("upgrade websocket doesn't exist\n");
        return -1;
    }

    printf("key is: %s\n", res);
    int total_len = value_len+GUID_LEN-1;
    res = realloc(res, total_len);
    memcpy(res+value_len-1, GUID, GUID_LEN);

    char* hash = calloc(HASH_LEN, sizeof(char)); // needs to be freed
    SHA1(res, total_len-1, hash); ///must not include the null char

    char* base64EncodeOutput = Base64Encode(hash, HASH_LEN);

    free(hash);
    free(res);

    char* response = calloc(150, sizeof(char));
    sprintf(response, 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: WebSocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", base64EncodeOutput);
    send(sockfd, response, strlen(response), 0);

    printf("response\n%s", response);

    free(response);
    free(base64EncodeOutput);

    printf("sleeping ... \n");
    sleep(5);
    printf("back again ... \n\n\n");


    close(sockfd);
    return 0;
}


int main() {
    int server_socket, new_socket;
    struct sockaddr_in server_addr, new_addr;
    socklen_t addr_size;
    char buffer[1024];
    
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

    // new_socket = accept(server_socket, (struct sockaddr*)&new_addr, &addr_size);  // Accept the data packet from client
    while(1){
        new_socket = accept(server_socket, (struct sockaddr*)&new_addr, &addr_size);
        memset(buffer, 0, sizeof(buffer));
        recv(new_socket, buffer, 1024, 0);
        // Receive data from the client and print it
        printf("Data received: %s\n", buffer);

        handle_handshake(new_socket, buffer);

        close(new_socket);
        
    }
    return 0;
}



