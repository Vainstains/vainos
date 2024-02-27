#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "../types.h"
#include "../drivers/disk.h"
#include "../libc/mem.h"
#define FILESYSTEM_BACKEND_FAT16 0

typedef struct {
    bool allOK;
    byte backend;

    void *backendSpecificData;
} FSInfo;


bool fsInitDisk(DiskInfo *disk, byte backend);

bool fsCreateFile(char *path);
bool fsOpenFile(char *path);
bool fsCloseFile(char *path);


#endif // FILESYSTEM_H