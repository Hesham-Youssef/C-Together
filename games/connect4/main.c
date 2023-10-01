#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#define ROWS 6
#define COLS 7
#define REALROWS 7
#define HEIGHT 700
#define WIDTH 700

int checkWin(char board[REALROWS][COLS], char player) {
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



void drawer(SDL_Renderer* renderer, char currentPlayer, SDL_Texture* tableTexture, char board[REALROWS][COLS]){
    SDL_RenderClear(renderer);

    // SDL_SetTextureColorMod(tableTexture, (currentPlayer == 'X')*255, (currentPlayer == 'O')*255, 0);

    // Draw the filled rectangle

    SDL_Rect rect = {.w = 100, .h = 100};

    printf("current player is %c\n", currentPlayer);
    for (int i = 0; i < COLS; i++) {
        for (int j = 0; j < ROWS; j++) {
            if(board[j][i] == ' ')
                continue;
            rect.x = i * 100;
            rect.y = (j * 100) + 100;
            rect.w = 100;
            rect.h = 100;
            SDL_SetRenderDrawColor(renderer, 255*(board[j][i]=='O'), 255*(board[j][i]=='X'), 0, 255);
            SDL_RenderFillRect(renderer, &rect);
        }
        printf("\n");
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    
    // Draw a blue rectangle
    SDL_RenderCopy(renderer, tableTexture, NULL, NULL);

    // Update the screen
    SDL_RenderPresent(renderer);
}


char controller(char board[REALROWS][COLS], char currentPlayer){
    // Find the first empty row in the selected column
    bool done = false;
    int move;
    SDL_Event event;
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
        return currentPlayer;
    board[REALROWS-(board[REALROWS-1][move]++)-2][move]= currentPlayer;


    // Check if the current player has won
    if (checkWin(board, currentPlayer)) {
        printf("Player %c wins!\n", currentPlayer);
        // return -1*currentPlayer;
    }

    // Check if the board is full (tie game)
    int isFull = 1;
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            if (board[i][j] == ' ') {
                isFull = 0;
                break;
            }
        }
    }

    if (isFull) {
        printf("It's a tie!\n");
        // return -2;
    }

    currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';

    return currentPlayer;
}


int main(){
    char board[REALROWS][COLS];
    char currentPlayer = 'X';

    for (int i = 0; i < REALROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            board[i][j] = ' '*(i!=REALROWS-1);
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        printf("SDL_ttf could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_PNG) == 0) {
        printf("SDL_image could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }


    SDL_Window* window = NULL;
    window = SDL_CreateWindow("Simple SDL2 Application", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = NULL;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    

    SDL_Surface* tableSurface = IMG_Load("assets/table.png");
    if (tableSurface == NULL) {
        printf("Image could not be loaded! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Texture* tableTexture = SDL_CreateTextureFromSurface(renderer, tableSurface);
    SDL_FreeSurface(tableSurface);

    // SDL_Surface* Surface = IMG_Load("assets/table.png");
    // if (tableSurface == NULL) {
    //     printf("Image could not be loaded! SDL_Error: %s\n", SDL_GetError());
    //     return 1;
    // }
    // SDL_Texture* tableTexture = SDL_CreateTextureFromSurface(renderer, tableSurface);
    // SDL_FreeSurface(tableSurface);


    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    while (1) {
        printf("Connect Four\n");

        drawer(renderer, currentPlayer, tableTexture, board);

        currentPlayer = controller(board, currentPlayer);
        if(currentPlayer < 0)
            break;
    }

    switch(currentPlayer){
        case -1:
            printf("quitting");
            break;
        case -2:
            printf("it is a tie");
            break;
        default:
            //display winner
            printf("the winner is %c\n", currentPlayer*-1);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    IMG_Quit();
    TTF_Quit();

    return 0;
}