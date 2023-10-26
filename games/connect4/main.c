#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define ROWS 6
#define COLS 7
#define REALROWS 7
#define HEIGHT 700
#define WIDTH 700


// void* get_state(void* args);
void* update_state(void* args);
// void* create_room(void* args);
// void* join_room(void* args);


struct sockaddr_in server_addr;
int address_length;
char current_player;
// drawer(renderer, current_player, tableTexture, board);


void init_connection(int client_socket){
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    address_length = sizeof(server_addr);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to server");
        close(client_socket);
        exit(1);
    }
}

int init_sdl(SDL_Window** window, SDL_Renderer** renderer, SDL_Surface** tableSurface, SDL_Texture** tableTexture){
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() < 0) {
        printf("SDL_ttf could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_PNG) == 0) {
        printf("SDL_image could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    *window = SDL_CreateWindow("Simple SDL2 Application", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (*window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (*renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);

    *tableSurface = IMG_Load("assets/table.png");
    if (*tableSurface == NULL) {
        printf("Image could not be loaded! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    *tableTexture = SDL_CreateTextureFromSurface(*renderer, *tableSurface);
    SDL_FreeSurface(*tableSurface);

    SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);
}


int checkWin(char board[][COLS], char player) {
    // Check horizontally
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS - 3; j++) {
            if (board[i][j] == player && board[i][j+1] == player &&
                board[i][j+2] == player && board[i][j+3] == player) {
                return 1;
            }
        }
    }

    // Check vertically
    for (int i = 0; i < ROWS - 3; i++) {
        for (int j = 0; j < COLS; j++) {
            if (board[i][j] == player && board[i+1][j] == player &&
                board[i+2][j] == player && board[i+3][j] == player) {
                return 1;
            }
        }
    }

    // Check diagonally (bottom-left to top-right)
    for (int i = 3; i < ROWS; i++) {
        for (int j = 0; j < COLS - 3; j++) {
            if (board[i][j] == player && board[i-1][j+1] == player &&
                board[i-2][j+2] == player && board[i-3][j+3] == player) {
                return 1;
            }
        }
    }

    // Check diagonally (top-left to bottom-right)
    for (int i = 0; i < ROWS - 3; i++) {
        for (int j = 0; j < COLS - 3; j++) {
            if (board[i][j] == player && board[i+1][j+1] == player &&
                board[i+2][j+2] == player && board[i+3][j+3] == player) {
                return 1;
            }
        }
    }

    return 0;
}



void drawer(SDL_Renderer* renderer, char currentPlayer, SDL_Texture* tableTexture, char board[][COLS]){
    SDL_RenderClear(renderer);

    // SDL_SetTextureColorMod(tableTexture, (currentPlayer == 'X')*255, (currentPlayer == 'O')*255, 0);
    SDL_Rect rect = {.w = 100, .h = 100};
    printf("current player is %c\n", currentPlayer);
    for (int i = 0; i < COLS; i++) {
        for (int j = 0; j < ROWS; j++) {
            if(board[j][i] == 0)
                continue;
            rect.x = i * 100;
            rect.y = (j * 100) + 100;
            rect.w = 100;
            rect.h = 100;
            SDL_SetRenderDrawColor(renderer, 255*(board[j][i]==1), 255*(board[j][i]==2), 0, 255);
            SDL_RenderFillRect(renderer, &rect);
            
        }
        printf("\n");
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderCopy(renderer, tableTexture, NULL, NULL);
    SDL_RenderPresent(renderer);
}


void parse_state_to_string(char board[][COLS], char turn, char matrixString[500]){
    int offset = 0;
    for (int i = 0; i < REALROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            // Format the matrix element and append it to the string
            ///change 500 and give sizeof(matrixstring) through parameters
            int len = snprintf(matrixString + offset, 500 - offset, "%d ", board[i][j]);
            offset += len;
        }
        // Add a newline character to separate rows
    }
    *(matrixString+offset)= turn;
}


char parse_string_to_state(char board[][COLS], char buffer[500]){
    
    int row = 0, col = 0;
    char *token = strtok(buffer, " \n"); // Tokenize using space and newline as delimiters
    int temp;
    
    while (token != NULL && row < 7) {
        sscanf(token, "%d", &temp);
        board[row][col] = temp;
        col++;

        if (col == 7) {
            col = 0;
            row++;
        }

        token = strtok(NULL, " \n");
    }
    return token[0];
}




char controller(char board[][COLS], int count, char my_turn){
    // Find the first empty row in the selected column
    bool done = false;
    int move;
    SDL_Event event;
    char msg[100];
    int msg_len = sprintf(msg, "Count is %d", count);
    for (int i = 0; i < REALROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("%d ", board[i][j]);
        }
        printf("\n");
    }

    while(!done){
        SDL_WaitEvent(&event);
        switch(event.type){
            case SDL_QUIT:
                done = true;
                return -1;
            case SDL_MOUSEBUTTONDOWN:
                if(event.button.button == SDL_BUTTON_LEFT){
                    int x, y;
                    SDL_GetMouseState(&x,&y);

                    printf("x:%d   y:%d\n", x, y);

                    if(current_player != my_turn){
                        printf("not your turn\n");
                        continue;
                    }

                    move  = x/100;
                    if (y < 100 || move < 0 || move >= COLS) {
                        printf("Invalid column\n");
                        continue;
                    }
                    done = true;
                }
                break;
        }
    }

    // Check if the column is valid
    if(board[REALROWS-1][move] >= 6)
        return current_player;
    board[REALROWS-(board[REALROWS-1][move]++)-2][move]= current_player;

    // Check if the current player has won
    if (checkWin(board, current_player)) {
        printf("Player %c wins!\n", current_player);
        // return -1*currentPlayer; ////open later
    }

    // Check if the board is full (tie game)
    int isFull = 1;
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            if (board[i][j] == 0) {
                isFull = 0;
                break;
            }
        }
    }

    if (isFull) {
        printf("It's a tie!\n");
        // return -2;  ////open later
    }

    current_player = (current_player == 1)? 2 : 1;

    return current_player;
}




int main(int argc, char* argv[]){
    char board[REALROWS][COLS];

    for (int i = 0; i < REALROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            board[i][j] = 0;
        }
    }

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* tableSurface; 
    SDL_Texture* tableTexture;


    init_sdl(&window, &renderer, &tableSurface, &tableTexture);

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Error creating socket");
        exit(1);
    }
    init_connection(client_socket);

    char buffer[100];
    bzero(buffer, 100);
    sprintf(buffer, "connect4 %s %s", argv[1], argv[2]);
    send(client_socket, buffer, strlen(buffer), 0);

    bzero(buffer, 100);
    recv(client_socket, buffer, sizeof(buffer), 0);

    bool online = true;
    int count = 0;

    char my_turn = (argv[1][0]=='c')? 1 : 2;

    current_player = 1;    

    while (current_player != -1) {
        printf("Connect Four\n");
        
        drawer(renderer, current_player, tableTexture, board);

        current_player = controller(board, count, my_turn);
        if(current_player < 0)
            break;
    }

    switch(current_player){
        case -1:
            printf("quitting");
            break;
        case -2:
            printf("it is a tie");
            break;
        default:
            //display winner
            printf("the winner is %c\n", current_player*-1);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    IMG_Quit();
    TTF_Quit();
    
    return 0;
}