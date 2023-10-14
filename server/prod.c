#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

// Define a structure for the message
struct Message {
    long mtype;
    char mtext[100];
};

int main() {
    key_t key;
    int msgid;
    struct Message message;

    // Generate a unique key for the message queue
    key = ftok("connect4", 'A');

    // Create a message queue (or get the ID if it already exists)
    msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }

    // Prepare the message
    message.mtype = 1; // Message type (can be used for filtering)
    strcpy(message.mtext, "connect4!");

    // Send the message to the message queue
    if (msgsnd(msgid, &message, sizeof(message), 1) == -1) {
        perror("msgsnd");
        exit(1);
    }

    printf("Producer: Sent message to the queue\n");

    return 0;
}
