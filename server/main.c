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

#include "Server.h"
#include "LinkedList.h"

// void bzero(void *s, size_t n);
#define bzero(s, n) memset((s), 0, (n))
// void bcopy(const void *s1, void *s2, size_t n);
#define bcopy(s1, s2, n) memmove((s2), (s1), (n))

#define CONTROLLEN CMSG_LEN(sizeof(int))
#define MAX_APP_NAME    32

// Define a structure to store name-PID mappings
typedef struct AppInfo {
    pid_t pid;
    char name[MAX_APP_NAME];
    int sockfd;
}AppInfo;

Node* apps = NULL;
pthread_mutex_t apps_mutex;
sem_t apps_sem;

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



void launch(struct Server* server){
    // struct ProcessInfo processes[100];
    int address_length = sizeof(server->address);
    int count = 0;

    // struct sockaddr_un addr;
    bool process_created = false;
    // char app_msg[100];


    struct msghdr msg;
    struct iovec iov;
    char dummy = 'D'; ////use this to send to app_handler whether we join or create
    char control[CONTROLLEN];
    struct cmsghdr *cmptr;

    iov.iov_base = &dummy;
    iov.iov_len = 1;
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
    char app_name[MAX_APP_NAME];
    int new_socket;
    while(1){    
        printf("===== waiting for connection =====\n");
        currApp = NULL;
        new_socket = accept(server->socket, (struct sockaddr*)&server->address, (socklen_t*)&address_length);
        
        offset = 0;
        bzero(app_name, MAX_APP_NAME);
        while((offset < MAX_APP_NAME)){
            read(new_socket, app_name+offset, 1);
            if(app_name[offset] == ' '){
                app_name[offset] = 0;
                break;
            }
            offset++;
        }

        if(offset == MAX_APP_NAME){
            send(new_socket, "App name is too long, must be shorter than 32 chars ;()\n", 57, 0);
            close(new_socket);
            continue;
        }
        printf("%s\n", app_name);

        pthread_mutex_lock(&apps_mutex);
        currApp = find_app_with_name(app_name);
        pthread_mutex_unlock(&apps_mutex);
        
        if(currApp == NULL){
            currApp = create_app_handler(app_name, new_socket);
            process_created = true;
            pthread_mutex_lock(&apps_mutex);
            append(&apps, currApp);
            pthread_mutex_unlock(&apps_mutex);
            sem_post(&apps_sem);
        }

        *((int *)CMSG_DATA(cmptr)) = new_socket;
        
        if (sendmsg(currApp->sockfd, &msg, MSG_NOSIGNAL) != 1) {
            ///kill child (to be done)
            kill(currApp->pid, SIGTERM);
            perror("sendmsg");
        }

        close(new_socket);
    }
}



void* sig_handler_thread_func(void* arg){
    int status;
    pid_t child_pid;
    /////////freee the app info struct
    while (1) {
        // pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        sem_wait(&apps_sem);
        child_pid = waitpid(-1, &status, 0);
        printf("after waitpid \n");
        if (child_pid == -1) {
            perror("Wait failed");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
            printf("Child process %d exited with status %d\n", child_pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child process %d terminated by signal %d\n", child_pid, WTERMSIG(status));
        }
        pthread_mutex_lock(&apps_mutex);
        AppInfo* appinfo = (AppInfo*)removeElement(&apps, child_pid);
        free(appinfo);
        pthread_mutex_unlock(&apps_mutex);
    }
    printf("hello before thread return\n");
    return NULL;
}


/// there is two leaks
/// one associated with the thread not cleaning up  (found but yet to be fixed)
/// the other is associated with scope              (not found yet)

// void term(){
//     printf("hello from term\n");
//     raise(SIGTERM);
// }


int main(){
    // signal(SIGCHLD, sigchld_handler);
    sem_init(&apps_sem, 0, 0);
    signal(SIGPIPE, SIG_IGN);
    // signal(SIGINT, term);
    srand(time(NULL));
    pthread_t signal_handler_thread;
    pthread_create(&signal_handler_thread, NULL, sig_handler_thread_func, NULL);
    pthread_mutex_init(&apps_mutex, NULL);
    // send_msg_to_app("connect4", "resputian\n");
    struct Server* server = server_constructor(AF_INET, SOCK_STREAM, 0,
        INADDR_ANY, 8080, 10, launch);
    server->launch(server);
    pthread_cancel(signal_handler_thread);
    pthread_join(signal_handler_thread, NULL);
    free(server);
    pthread_mutex_destroy(&apps_mutex);
    sem_destroy(&apps_sem);
    return 0;
}