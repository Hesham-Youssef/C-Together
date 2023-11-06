#include "Websocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <unistd.h>
#include <arpa/inet.h>

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

    // printf("key is: %s\n", res);
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

    // printf("response\n%s", response);

    free(response);
    free(base64EncodeOutput);
    return 0;
}



int get_frame_type(const char* frame, int frame_len){
    if (frame_len < 2) {
        // Incomplete header, cannot parse
        return -1;
    }

    uint8_t fin = (frame[0] & 0x80) >> 7;
    uint8_t opcode = frame[0] & 0x0F;
    uint8_t mask = (frame[1] & 0x80) >> 7;
    uint8_t payload_length = frame[1] & 0x7F;
    size_t header_length = 2;
    size_t extended_payload_length = 0;

    if (payload_length == 126) {
        if (frame_len < 4) {
            // Incomplete header, cannot parse
            return -1;
        }
        extended_payload_length = (uint64_t)(frame[2] << 8 | frame[3]);
        header_length = 4;
    } else if (payload_length == 127) {
        if (frame_len < 10) {
            // Incomplete header, cannot parse
            return -1;
        }
    }

    if (mask) {
        if (frame_len < header_length + 4) {
            // Incomplete header, cannot parse
            return -1;
        }
    }
    

    return opcode;
}


#define PONG_LEN 2

int handle_ping(int sockfd, char* frame){
    frame[0] = 0x8A;  // FIN bit set (0x8) and Opcode for Pong (0xA).

    // Set the Mask bit to 0 (no masking of payload).
    frame[1] = 0x00;

    // Set the Payload Length to 0, as there is no payload.
    frame[2] = 0x00;

    // Increase the frame length to reflect the frame size.
    // *frame_len = 2;  // Frame length is 2 bytes (2 bytes of the frame header).
    
    int res = send(sockfd, frame, PONG_LEN, 0);

    if(res <= 0){
        perror("sending pong");
        return -1;
    }

    return res;
}







