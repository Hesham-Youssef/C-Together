#include <stdio.h>
#include <stdlib.h>

#include "LinkedList.h"

// Function to create a new node
Node* createNode(void* data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode) {
        newNode->data = data;
        newNode->next = NULL;
    }
    return newNode;
}

// Function to add a new element at the end of the list
void append(Node** head, void* data) {
    Node* newNode = createNode(data);
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    if (*head == NULL) {
        *head = newNode;
    } else {
        Node* current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }
}


// Function to remove an element from the list
void removeElement(Node** head, void* target) {
    Node* current = *head;
    Node* prev = NULL;

    while (current != NULL) {
        if (current->data == target) {
            if (prev == NULL) {
                *head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return; // Element removed
        }

        prev = current;
        current = current->next;
    }
}

// Function to pop the first element from the list
void* pop(Node** head) {
    if (*head == NULL) {
        return NULL; // List is empty
    }

    Node* current = *head;
    *head = current->next;
    void* data = current->data;
    free(current);
    return data;
}

// Function to check if the list is empty
int is_empty(Node* head) {
    return head == NULL;
}

// Function to print the elements of the list
void printList(Node* head) {
    Node* current = head;
    while (current != NULL) {
        printf("%p -> ", current->data);
        current = current->next;
    }
    printf("NULL\n");
}

// Function to free the memory of the list
void freeList(Node* head) {
    Node* current = head;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
        free(temp);
    }
}
