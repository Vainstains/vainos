#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "../types.h"
#include "../drivers/disk.h"
#include "../libc/mem.h"
#include "fat16.h"

#define FILESYSTEM_BACKEND_FAT16 1

typedef struct {
    bool allOK;
    byte backend;

    void *info;
} FSInfo;


bool fsInit(FSInfo *fs, void *info, byte backend);

bool fsCreateFile(FSInfo *fs, char *path);
bool fsCreateDirectory(FSInfo *fs, char *path);
bool fsWriteFile(FSInfo *fs, char *path, byte *buffer, uint32_t nbytes);
bool fsReadFile(FSInfo *fs, char *path, byte *buffer, uint32_t nbytes);

bool fsIsFile(FSInfo *fs, char *path);
bool fsPathExists(FSInfo *fs, char *path);
uint32_t fsFileSize(FSInfo *fs, char *path);
char *fsEnumerateDirectoryAlloc(FSInfo *fs, char *path);

void fsJoinPaths(char *pathA, char *pathB, char *pathOut);


#endif // FILESYSTEM_H