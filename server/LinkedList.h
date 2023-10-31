#ifndef LinkedList_h
#define LinkedList_h
#include <stdio.h>
#include <stdlib.h>

// Define a doubly linked list node structure
typedef struct Node {
    void* data;
    struct Node* next;
    struct Node* prev; // Add a 'prev' pointer for the previous node
} Node;


// Function to add a new element at the end of the list
void append(Node** head, void* data);

// Function to search for an element in the list
Node* search(Node* head, int target);

// Function to remove an element from the list
void* removeElement(Node** head, int target);

///when removing the element it only compares the first int in the struct
// Function to remove an element from the list
void removeNode(Node** head, Node* node);

// Function to print the elements of the list
void printList(Node* head);

// Function to free the memory of the list
void freeList(Node* head);

// Function to pop the first element from the list
void* pop(Node** head);

// Function to check if the list is empty
int is_empty(Node* head);

#endif


// int main() {
//     Node* head = NULL;

//     int value1 = 42;
//     int value2 = 77;
//     char* str = "Hello, World!";

//     append(&head, &value1);
//     append(&head, &value2);
//     append(&head, str);

//     printf("Original List: ");
//     printList(head);

//     void* searchValue = &value2;
//     Node* searchResult = search(head, searchValue);
//     if (searchResult) {
//         printf("Found %p in the list.\n", searchValue);
//     } else {
//         printf("%p not found in the list.\n", searchValue);
//     }

//     void* removeValue = str;
//     removeElement(&head, removeValue);
//     printf("List after removing %p: ", removeValue);
//     printList(head);

//     freeList(head); // Free memory when done

//     return 0;
// }
