// open file
// close file
// write
// read
// get file info
// file exists
// directory exists
// get directory contents

#include "../types.h"

typedef struct {
    uint16_t handle;
    uint32_t pos;
    char flags;
} fileHandle_t;

typedef struct {
    char flags;
} fsInfo_t;

void openFile(fileHandle_t *handle, char *path, char flags) {
    return; //TODO: implement
}

void closeFile(fileHandle_t *handle) {
    return; //TODO: implement
}

void writeBytes(fileHandle_t *handle, char *source, int nbytes) {
    return; //TODO: implement
}

void readBytes(fileHandle_t *handle, char *source, int nbytes) {
    return; //TODO: implement
}

void fileInfo(char *path, fsInfo_t *fsinfo) {
    return; //TODO: implement
}

void dirInfo(char *path) {
    return; //TODO: implement
}

void dirContents(char *path) {
    return; //TODO: implement
}