// Save as forever.c
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void handler(int sig) {
    printf("Receives SIGINT!! IGNORE IT :)\n");
    fflush(stdout);
}

int main() {
    signal(SIGINT, handler);
    while (1) {
        pause();
    }
    return 0;
}
