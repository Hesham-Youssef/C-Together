#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <semaphore.h>

#include <signal.h>           /* Definition of SIG_* constants */
#include <sys/syscall.h>      /* Definition of SYS_* constants */
#include <unistd.h>

#include "Server.h"
#include "LinkedList.h"

// void bzero(void *s, size_t n);
#define bzero(s, n) memset((s), 0, (n))
// void bcopy(const void *s1, void *s2, size_t n);
#define bcopy(s1, s2, n) memmove((s2), (s1), (n))

#define CONTROLLEN CMSG_LEN(sizeof(int))
#define MAX_APP_NAME    50
#define MAX_BUFFER_LEN    100
#define MAX_MSG_LEN     32

// Define a structure to store name-PID mappings
typedef struct AppInfo {
    pid_t pid;
    char name[MAX_APP_NAME];
    int sockfd;
}AppInfo;

Node* apps = NULL;

AppInfo* find_app_with_name(char app_name[]){
    Node* curr = apps;
    int name_len = strlen(app_name);
    while(curr != NULL){
        printf("helloo curr: %s\napp_name: %s\n", ((AppInfo*)(curr->data))->name, app_name);
        if(strcmp(((AppInfo*)(curr->data))->name, app_name) == 0){
            printf("found curr: %s\napp_name: %s\n", ((AppInfo*)(curr->data))->name, app_name);
            return ((AppInfo*)(curr->data));
        }
        curr = curr->next;
    }
    return NULL;
}



AppInfo* create_app_handler(char* app_name, int client_sockfd){
    // printf("hello world inside create\n");
    int sockfd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) < 0) {
        perror("socketpair");
        exit(1);
    }

    pid_t child_pid;
    child_pid = fork();
    if (child_pid == -1) {
        perror("Fork failed");
        exit(1);
    }

    if (child_pid == 0) {
        // strcpy(processes[0].name, app_name);
        // processes[0].pid = getpid();
        close(sockfd[0]);

        char sockfd_str[10];
        bzero(sockfd_str, 10);
        snprintf(sockfd_str, 10, "%d", sockfd[1]);

        char client_sockfd_str[10];
        bzero(client_sockfd_str, 10);
        snprintf(client_sockfd_str, 10, "%d", client_sockfd);
        char *args[] = {"./app_handler", app_name, sockfd_str, client_sockfd_str, NULL}; 
        if (execvp("./app_handler", args) == -1) {
            perror("Exec failed");
            exit(1);
        }
    } else {
        close(sockfd[1]);
        AppInfo* app = calloc(1, sizeof(AppInfo)); //check if successful
        strcpy(app->name, app_name);
        app->pid = child_pid;
        app->sockfd = sockfd[0];

        return app;
    }
}

void signal_handler(){
    int status;
    pid_t child_pid;
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("after waitpid \n");
        if (WIFEXITED(status)) {
            printf("Child process %d exited with status %d\n", child_pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child process %d terminated by signal %d\n", child_pid, WTERMSIG(status));
        }
        AppInfo* appinfo = (AppInfo*)removeElement(&apps, child_pid);
        free(appinfo);
    }
    printf("hello before thread return\n");
}


void launch(struct Server* server){
    // struct ProcessInfo processes[100];
    int address_length = sizeof(server->address);
    int count = 0;

    // struct sockaddr_un addr;
    bool process_created = false;
    // char app_msg[100];


    struct msghdr msg = {0};
    struct iovec iov;

    char control[CONTROLLEN];
    struct cmsghdr *cmptr;

    iov.iov_len = 32;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CONTROLLEN;
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;

    AppInfo* currApp = NULL;
    int offset;
    char buffer[MAX_BUFFER_LEN];
    char app_name[MAX_APP_NAME];
    char params[MAX_MSG_LEN];

    int new_socket;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGCHLD, &sa, NULL);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    sigprocmask(SIG_BLOCK, &mask, NULL);
    
    sigset_t pending_signals;

    while(1){    
        printf("===== waiting for connection =====\n");
        currApp = NULL;
        new_socket = accept(server->socket, (struct sockaddr*)&server->address, (socklen_t*)&address_length);
        
        sigpending(&pending_signals);
        if (sigismember(&pending_signals, SIGCHLD)) {
            printf("SIGCHLD is pending\n");
            signal_handler();
        }

        offset = 0;
        memset(buffer, 0, MAX_BUFFER_LEN);
        int read_len = 0;
        do{
            read_len = recv(new_socket, buffer+offset, 1, 0);
        }while((read_len > 0) && (offset < MAX_BUFFER_LEN) && (buffer[offset++] != '\n'));
        buffer[offset] = 0;
  
        if(offset == MAX_APP_NAME){
            // send(new_socket, "App name is too long, must be shorter than 32 chars ;()\n", 57, 0);
            close(new_socket);
            continue;
        }
        
        memset(app_name, 0, MAX_APP_NAME);
        memset(params, 0, MAX_MSG_LEN);
        sscanf(buffer, "GET /%[^/]/%s HTTP", app_name, params);

        printf("app_name: %s\n", app_name);
        printf("params: %s\n", params);

        currApp = find_app_with_name(app_name);
        if(currApp == NULL){
            currApp = create_app_handler(app_name, new_socket);
            process_created = true;
            append(&apps, currApp);
        }

        *((int *)CMSG_DATA(cmptr)) = new_socket;
        iov.iov_base = params;
        
        if (sendmsg(currApp->sockfd, &msg, MSG_NOSIGNAL) != 1) {
            ///kill child (to be done)
            // kill(currApp->pid, SIGTERM);
            // perror("sendmsg");

            //there is a chance that the process just turned off
            // send(new_socket, "Error occurred, try again later :(", 35, 0);  // jjjjjj
        }

        close(new_socket);
    }

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}




/// there is two leaks
/// one associated with the thread not cleaning up  (found but yet to be fixed)
/// the other is associated with scope              (not found yet)


int main(){
    // signal(SIGCHLD, sigchld_handler);
    signal(SIGPIPE, SIG_IGN);
    // signal(SIGINT, term);
    srand(time(NULL));
    struct Server* server = server_constructor(AF_INET, SOCK_STREAM, 0,
        INADDR_ANY, 8080, 10, launch);
    server->launch(server);
    free(server);
    return 0;
}