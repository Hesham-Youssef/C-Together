#include <stdio.h>
#include <stdlib.h>
#include "Queue.h"

// Function to create an empty queue
struct Queue* createQueue() {
    struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue));
    if (!queue) {
        perror("Memory allocation error");
        exit(1);
    }
    queue->front = queue->rear = NULL;
    return queue;
}

// Function to enqueue an element into the queue
void enqueue(struct Queue* queue, int data) {
    // Create a new node
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    if (!newNode) {
        perror("Memory allocation error");
        exit(1);
    }
    newNode->data = data;
    newNode->next = NULL;

    // If the queue is empty, set both front and rear to the new node
    if (queue->rear == NULL) {
        queue->front = queue->rear = newNode;
        return;
    }

    // Otherwise, add the new node to the end and update the rear pointer
    queue->rear->next = newNode;
    queue->rear = newNode;
}

// Function to dequeue an element from the queue
int dequeue(struct Queue* queue) {
    if (queue->front == NULL) {
        fprintf(stderr, "Queue is empty.\n");
        exit(1);
    }

    // Remove the front node and update the front pointer
    struct Node* temp = queue->front;
    int data = temp->data;
    queue->front = queue->front->next;

    // If the queue becomes empty, also update the rear pointer
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(temp);
    return data;
}

// Function to check if the queue is empty
int isEmpty(struct Queue* queue) {
    return (queue->front == NULL);
}
