#ifndef Websocket_h
#define Websocket_h

#include <inttypes.h>
#include <stddef.h>

#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT 0x1
#define WS_OPCODE_BINARY 0x2
#define WS_OPCODE_CLOSE 0x8
#define WS_OPCODE_PING 0x9
#define WS_OPCODE_PONG 0xA


int handle_handshake(int sockfd, char* msg);
int get_frame_type(const char* frame, int frame_len);
int handle_ping(int sockfd, char* frame);

//TO DO: implement a ws_send_msg(char* msg, int msg_len)
int ws_send_msg(char* msg, int msg_len);
int send_websocket_message(int sockfd, const char *message, size_t length);
void ws_send_ping(int sockfd, char* msg, int msg_len);

#endif