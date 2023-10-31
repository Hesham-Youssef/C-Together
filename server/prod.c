#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main() {
    int* ptr = NULL;
    if(1){
        int x = 5;
        ptr = &x;
    }
    printf("%d\n", *ptr);
    return 0;
}
