#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    printf("Starting temp daemon...\n");
    // Your daemon code or call to daemon initialization here
    while (1) {
        // do background tasks
        sleep(5);
    }
    return 0;
}
