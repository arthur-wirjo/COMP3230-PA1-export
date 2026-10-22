// Save as loopf.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int sec = (argc > 1) ? atoi(argv[1]) : 10;
    for (int i = 0; i < sec; i++) {
        sleep(1);
    }
    return 0;
}
