#ifndef Queue_h
#define Queue_h
#include <stdio.h>
#include <stdlib.h>

// Define the structure for a node in the linked list
struct Node {
    int data;
    struct Node* next;
};

// Define the structure for the linked list queue
struct Queue {
    struct Node* front;
    struct Node* rear;
};

// Function to create an empty queue
struct Queue* createQueue();

// Function to enqueue an element into the queue
void enqueue(struct Queue* queue, int data);

// Function to dequeue an element from the queue
int dequeue(struct Queue* queue);

// Function to check if the queue is empty
int isEmpty(struct Queue* queue);

#endif


// int main() {
//     struct Queue* queue = createQueue();

//     enqueue(queue, 10);
//     enqueue(queue, 20);
//     enqueue(queue, 30);

//     printf("Dequeued: %d\n", dequeue(queue));
//     printf("Dequeued: %d\n", dequeue(queue));

//     enqueue(queue, 40);
//     enqueue(queue, 50);

//     while (!isEmpty(queue)) {
//         printf("Dequeued: %d\n", dequeue(queue));
//     }
//     return 0;
// }