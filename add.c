// Save as add.c
#include <stdio.h>
#include <stdlib.h>

int main() {
    volatile double sum = 0;
    // Adjust loop count if your CPU is much faster/slower (~1.3 seconds)
    for (long i = 0; i < 600000000L; i++) {
        sum += i * 0.5;
    }
    return 0;
}
