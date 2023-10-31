#include <stdio.h>
#include <stdlib.h>

// Define a doubly linked list node structure
typedef struct Node {
    void* data;
    struct Node* next;
    struct Node* prev; // Add a 'prev' pointer for the previous node
} Node;

// Function to create a new node
Node* createNode(void* data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode) {
        newNode->data = data;
        newNode->next = NULL;
        newNode->prev = NULL;
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
        newNode->prev = current;
    }
}

// Function to remove a specific node from the list
void removeNode(Node** head, Node* node) {
    if (*head == NULL || node == NULL) {
        return; // Nothing to remove
    }

    if (node->prev == NULL) {
        *head = node->next;
    } else {
        node->prev->next = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    free(node);
}

// Function to remove an element from the list using room number
void* removeElement(Node** head, int target) {
    Node* current = *head;

    while (current != NULL) {
        if (*((int*)(current->data)) == target) {
            if (current->prev == NULL) {
                *head = current->next;
            } else {
                current->prev->next = current->next;
            }
            if (current->next != NULL) {
                current->next->prev = current->prev;
            }
            void* data = current->data;
            free(current);
            return data; // Element removed
        }

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
    if (*head != NULL) {
        (*head)->prev = NULL;
    }
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
        printf("%p <-> ", current->data);
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

// Function to search for an element in the list using room number
Node* search(Node* head, int target) {
    Node* current = head;
    while (current != NULL) {
        if (*((int*)(current->data)) == target) {
            return current;
        }
        current = current->next;
    }
    return NULL; // Element not found
}
