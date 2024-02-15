#include "../types.h"

void memoryCopy(char *source, char *dest, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

void memorySet(char *dest, char val, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = val;
    }
}