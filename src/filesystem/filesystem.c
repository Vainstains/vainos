#include "filesystem.h"

bool fsInit(FSInfo *fs, void *info, byte backend) {
    switch (backend) {
        case FILESYSTEM_BACKEND_FAT16:
            fs->allOK = true;
            fs->backend = backend;
            fs->info = info;
            return true;
    }
    return false;
}

bool fsCreateFile(FSInfo *fs, char *path) {
    if (!fs->allOK)
        return false;
    switch (fs->backend)
    {
        case FILESYSTEM_BACKEND_FAT16:
            fat16CreateFile((Fat16FilesystemInfo*)(fs->info), path);
            return true;
    }
}

bool fsCreateDirectory(FSInfo *fs, char *path) {
    if (!fs->allOK)
        return false;
    switch (fs->backend)
    {
        case FILESYSTEM_BACKEND_FAT16:
            fat16CreateDirectory((Fat16FilesystemInfo*)(fs->info), path);
            return true;
    }
}

bool fsWriteFile(FSInfo *fs, char *path, byte *buffer, uint32_t nbytes) {
    if (!fs->allOK)
        return false;
    switch (fs->backend)
    {
        case FILESYSTEM_BACKEND_FAT16:
            fat16WriteFile((Fat16FilesystemInfo*)(fs->info), path, buffer, nbytes);
            return true;
    }
}

bool fsReadFile(FSInfo *fs, char *path, byte *buffer, uint32_t nbytes) {
    if (!fs->allOK)
        return false;
    switch (fs->backend)
    {
        case FILESYSTEM_BACKEND_FAT16:
            fat16ReadFile((Fat16FilesystemInfo*)(fs->info), path, buffer, nbytes);
            return true;
    }
}

bool fsIsFile(FSInfo *fs, char *path) {
    if (!fs->allOK)
        return false;
    switch (fs->backend)
    {
        case FILESYSTEM_BACKEND_FAT16:
            
            return false;
    }
}

bool fsPathExists(FSInfo *fs, char *path) {
    if (!fs->allOK)
        return false;
    switch (fs->backend)
    {
        case FILESYSTEM_BACKEND_FAT16:
            
            return false;
    }
}

uint32_t fsFileSize(FSInfo *fs, char *path) {
    if (!fs->allOK)
        return false;
    switch (fs->backend)
    {
        case FILESYSTEM_BACKEND_FAT16:
            
            return 0;
    }
}

void fsJoinPaths(char *pathA, char *pathB, char *pathOut) {

}