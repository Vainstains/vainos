#include "fat16.h"

#include "../debug.h"
#include "../libc/mem.h"
#include "../libc/string.h"
#include "../drivers/vga.h"

// Note to self: pretty sure this is how Fat16 works at a high level:

// -The FAT is for storing info about blocks of data.
// -The Each fat entry is just an int16, that being the index to the next cluster.
//   (An index of 0xFFFF marks the end of a chain, 0x0000 marks an unused cluster)
// -The root directory is what organizes the FAT into files and directories
// -The root directory is not special, as in subdirectories are just files with their own "inner root directories"
//   (this means it is recursive, if handled correctly there can be little to no distinction between the root dir and a regular dir)
// -Entries point to the first FAT index, just follow the chain

static void readFAT(Fat16FilesystemInfo *fs, uint16_t *fat) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    diskRead(fs->disk, bootsector->reservedSectors, bootsector->sectorsPerFAT, (byte*) fat);
}

static void writeFAT(Fat16FilesystemInfo *fs, uint16_t *fat) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    diskWrite(fs->disk, bootsector->reservedSectors, bootsector->sectorsPerFAT, (byte*) fat);
}

static void readCluster(Fat16FilesystemInfo *fs, uint32_t cluster, byte *buffer) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT) + bootsector->rootDirCount / entriesPerSector;
    uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;
    diskRead(fs->disk, clusterStart, bootsector->sectorsPerCluster, buffer); 
}

static void writeCluster(Fat16FilesystemInfo *fs, uint32_t cluster, byte *buffer) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT) + bootsector->rootDirCount / entriesPerSector;
    uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;
    diskWrite(fs->disk, clusterStart, bootsector->sectorsPerCluster, buffer);
}

static void clearCluster(Fat16FilesystemInfo *fs, uint32_t cluster) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT) + bootsector->rootDirCount / entriesPerSector;
    uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;

    uint32_t clusterLength = bootsector->bytesPerSector*bootsector->sectorsPerCluster;
    byte *zeroes = (byte*)malloc(clusterLength);
    memset(zeroes, 0, clusterLength);
    diskWrite(fs->disk, clusterStart, bootsector->sectorsPerCluster, zeroes);
    free((void*)zeroes);
}

static uint32_t findFreeCluster(uint16_t *fat, uint32_t totalClusters) {
    for (uint32_t cluster = 2; cluster < totalClusters; cluster++) {
        if (fat[cluster] == 0x0000) {
            return cluster;
        }
    }
    return 0xFFFFFFFF;
}

static uint32_t chainLength(uint16_t *fat, uint32_t cluster) {
    uint32_t length = 1;
    uint16_t value;
    while (length < 8) {
        value = fat[cluster];
        if (value >= 0xFFF8) {
            break;
        }
        cluster = value;
        length++;
    }
    return length;
}

static void appendChain(Fat16FilesystemInfo *fs, uint16_t *fat, uint32_t cluster, uint32_t n) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t endCluster = cluster;
    while (fat[endCluster] < 0xFFF8) {
        endCluster = fat[endCluster];
    }
    for (uint32_t i = 0; i < n; i++) {
        uint32_t newCluster = findFreeCluster(fat, bootsector->sectorsPerFAT * bootsector->bytesPerSector / 2);
        if (newCluster == 0xFFFFFFFF) {
            return;
        }
        vgaWrite("new index: "); vgaWriteInt((uint16_t)newCluster); vgaNextLine();
        fat[endCluster] = (uint16_t)newCluster;
        fat[newCluster] = (i == n - 1) ? 0xFFFF : (uint16_t)(newCluster + 1);

        clearCluster(fs, newCluster);
        endCluster = newCluster;
    }
}

static void popChain(Fat16FilesystemInfo *fs, uint16_t *fat, uint32_t cluster, uint32_t n) {
    uint32_t endCluster = cluster;
    uint32_t prevCluster = 0xFFFFFFFF;

    while (fat[endCluster] < 0xFFF8) {
        prevCluster = endCluster;
        endCluster = fat[endCluster];
    }

    for (uint32_t i = 0; i < n && endCluster != 0xFFFFFFFF; i++) {
        uint32_t prev = prevCluster;

        prevCluster = fat[prevCluster];
        fat[prev] = 0x0000;

        endCluster = prev;
    }
}

static uint32_t fitChainToBytes(Fat16FilesystemInfo *fs, uint16_t *fat, uint32_t cluster, uint32_t nbytes) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t endCluster = cluster;
    while (fat[endCluster] < 0xFFF8) {
        endCluster = fat[endCluster];
    }

    uint32_t bytesPerCluster = bootsector->bytesPerSector * bootsector->sectorsPerCluster;
    uint32_t requiredClusters = (nbytes + bytesPerCluster - 1) / bytesPerCluster;

    uint32_t chainLen = chainLength(fat, cluster);

    if (chainLen == requiredClusters) {
        return chainLen;
    }

    if (chainLen > requiredClusters) {
        uint32_t excessClusters = chainLen - requiredClusters;
        popChain(fs, fat, cluster, excessClusters);
        return chainLen-excessClusters;
    }
    uint32_t missingClusters = requiredClusters - chainLen;
    appendChain(fs, fat, cluster, missingClusters);
    return chainLen+missingClusters;
}

static void readChain(Fat16FilesystemInfo *fs, uint16_t *fat, uint32_t cluster, byte *buffer, uint32_t nbytes) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t bufferOffset = 0;
    byte *bufferTmp = (byte*)malloc(bootsector->sectorsPerCluster * bootsector->bytesPerSector);
    
    while (cluster < 0xFFF8 && nbytes > 0) {
        uint32_t bytesToRead = nbytes < (bootsector->sectorsPerCluster * bootsector->bytesPerSector) ?
                               nbytes : (bootsector->sectorsPerCluster * bootsector->bytesPerSector);

        readCluster(fs, cluster, bufferTmp);
        memcpy(bufferTmp, buffer + bufferOffset, bytesToRead);
        bufferOffset += bytesToRead;
        nbytes -= bytesToRead;

        cluster = fat[cluster];
    }
    free((void*)bufferTmp);
}

static void writeChain(Fat16FilesystemInfo *fs, uint16_t *fat, uint32_t cluster, byte *buffer, uint32_t nbytes) {
    Fat16BootSector *bootsector = &(fs->bootsector);

    uint32_t bufferOffset = 0;

    while (cluster < 0xFFF8 && nbytes > 0) {
        uint32_t bytesToWrite = nbytes < (bootsector->sectorsPerCluster * bootsector->bytesPerSector) ?
                                nbytes : (bootsector->sectorsPerCluster * bootsector->bytesPerSector);                   
        writeCluster(fs, cluster, buffer + bufferOffset);

        bufferOffset += bytesToWrite;
        nbytes -= bytesToWrite;

        if (fat[cluster] >= 0xFFF8) {
            break;
        }

        cluster = fat[cluster];
    }
}

static void readBootsector(Fat16FilesystemInfo *fs) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    diskRead(fs->disk, 0, 1, (byte*) bootsector);
}

static inline bool entryIsAvailable(Fat16DirectoryEntry *entry) {
    return entry->fileName[0] == 0x00 || entry->fileName[0] == 0xE5;
}

static inline bool entryIsDirectory(Fat16DirectoryEntry *entry) {
    return (entry->flags & FAT16_FLAG_DIRECTORY);
}

static inline bool entryIsVolumeLabel(Fat16DirectoryEntry *entry) {
    return (entry->flags & FAT16_FLAG_VOLUMELABEL);
}

static inline bool entryIsFile(Fat16DirectoryEntry *entry) {
    return !entryIsDirectory(entry) && !entryIsVolumeLabel(entry) && !entryIsAvailable(entry);
}

static void createFilename83(char *name, char *ext, char *dst) {
    if (name[0] == '.' && (name[1] == '.' || name[1] == ' '))
    {
        dst[0] = '.';
        dst[1] = name[1];
        dst[2] = ' ';
        dst[3] = ' ';
        dst[4] = ' ';
        dst[5] = ' ';
        dst[6] = ' ';
        dst[7] = ' ';
        dst[8] = ' ';
        dst[9] = ' ';
        dst[10]= ' ';
        return;
    }
    uint8_t i = 0;
    for (; i < 8; i++) {
        dst[i] = name[i];
        if (dst[i] == '\0')
            break;
    }
    for (; i < 8; i++) {
        dst[i] = ' ';
    }

    for (; i < 11; i++) {
        dst[i] = ext[i-8];
        if (dst[i] == '\0')
            break;
    }
    for (; i < 11; i++) {
        dst[i] = ' ';
    }
    if (dst[0] == 0xE5)
        dst[0] = 0x05;
}

static void decodeFilename83(char *name, char *ext, char *name83) {
    uint8_t i = 0;
    
    for (; i < 8; i++) {
        if (name83[i] == ' ' || name83[i] == '\0')
            break;
        name[i] = name83[i];
    }
    name[i] = '\0';
    if (name[0] == 0x05)
        name[0] = 0xE5;
    
    i = 8;
    for (; i < 11; i++) {
        if (name83[i] == ' ' || name83[i] == '\0')
            break;
        ext[i - 8] = name83[i];
    }
    ext[i - 8] = '\0';
}

static void convertFilename83ToRegular(char *name83, char *dst) {
    if (name83[0] == '.' && (name83[1] == '.' || name83[1] == ' '))
    {
        dst[0] = '.';
        dst[1] = (name83[1] == '.')?'.':'\0';
        dst[2] = '\0';
        return;
    }
    uint8_t dstIdx = 0;
    uint8_t i = 0;
    for (; i < 8; i++) {
        if (name83[i] == ' ' || name83[i] == '\0')
            break;
        dst[dstIdx++] = name83[i];
    }
    dst[dstIdx++] = '.';
    i = 8;
    for (; i < 11; i++) {
        if (name83[i] == ' ' || name83[i] == '\0')
            break;
        dst[dstIdx++] = name83[i];
    }
    if (i == 8)
        dstIdx--;
    if (dst[0] == 0x05)
        dst[0] = 0xE5;
    dst[dstIdx] = 0; // null-term
}

static bool traversePath(Fat16FilesystemInfo *fs, uint16_t *fat, Fat16DirectoryEntry **dir, uint32_t *parentDirCluster, uint32_t *dirCluster, uint32_t *entryCount, char **path) {
    char name[9];
    int i, dirIdx;

    while (strcontains(*path, '/')) {
        i = 0;
        for (; i < 8; i++) {
            if ((*path)[i] == '/') break;
            name[i] = (*path)[i];
        }
        for (; i < 8; i++) {
            name[i] = ' ';
        }
        name[8] = '\0';
        *path = strchr(*path, '/') + 1;
        //vgaWrite("Entering directory: "); vgaWriteStatic(name, 8); vgaNextLine();
        if (name[0] == 0xE5)
            name[0] = 0x05;
        i = 0;
        dirIdx = -1;
        for (; i < *entryCount; i++) {
            if (entryIsDirectory(&((*dir)[i])))
            {
                //vgaWrite("    Possibly: "); vgaWriteStatic((*dir)[i].fileName, 8); vgaNextLine();
                if (strequal_nocase((*dir)[i].fileName, name, 8)) {
                    dirIdx = i;
                    break;
                }
            }
        }
        if (dirIdx < 0)
        {
            return false;
        }
        uint16_t newDirCluster = (*dir)[i].firstCluster;
        uint32_t bytes = chainLength(fat, newDirCluster) * (uint32_t)fs->bootsector.bytesPerSector * (uint32_t)fs->bootsector.sectorsPerCluster;
        free((void*)*dir);
        *dir = (Fat16DirectoryEntry*)malloc(bytes);
        readChain(fs, fat, newDirCluster, (byte*)*dir, bytes);
        *parentDirCluster = *dirCluster;
        *dirCluster = newDirCluster;
        *entryCount = bytes / sizeof(Fat16DirectoryEntry);
    }
    return true;
}

static bool createDirectory(Fat16FilesystemInfo *fs, uint16_t *fat, Fat16DirectoryEntry **dir, uint32_t parentDirCluster, uint32_t dirCluster, uint32_t entryCount, char *path) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    char name[9];

    int i = 0;
        for (; i < 8; i++) {
        if (path[i] == '\0') break;
        name[i] = path[i];
    }
    for (; i < 8; i++) {
        name[i] = ' ';
    }
    name[8] = '\0';
    if (name[0] == 0xE5)
        name[0] == 0x05;
    i = 0;
    for (; i < entryCount; i++) {
        if (entryIsDirectory(&((*dir)[i])))
        {
            if (strequal_nocase((*dir)[i].fileName, name, 8)) {
                vgaWriteln("Directory already exists");
                return false;
            }
        }
    }
    int dirIdx = -1;
    i = 0;
    for (; i < entryCount; i++) {
        if (entryIsAvailable(&((*dir)[i])))
        {
            dirIdx = i;
            break;
        }
    }
    if (dirIdx < 0)
    {
        vgaWriteln("No free entries");
        return false;
    }
    uint16_t cluster = dirCluster;
    if (name[0] == '.' && name[1] == '.') {
        cluster = parentDirCluster;
    } else {
        cluster = (uint16_t)findFreeCluster(fat, fatBytes / 2);
    }
    fat[cluster] = 0xFFFF;
    memcpy(name, (*dir)[dirIdx].fileName, 8);
    (*dir)[dirIdx].firstCluster = cluster;
    (*dir)[dirIdx].flags = FAT16_FLAG_DIRECTORY;
    (*dir)[dirIdx].size = (uint32_t)bootsector->bytesPerSector * (uint32_t)bootsector->sectorsPerCluster;
    if (dirCluster == 0)
        diskWrite(fs->disk, rootDirStart, sectors, (byte*)(*dir));
    else
        writeChain(fs, fat, dirCluster, (byte*)(*dir), (*dir)[dirIdx].size);

    return true;
}

static bool createFile(Fat16FilesystemInfo *fs, uint16_t *fat, Fat16DirectoryEntry **dir, uint32_t dirCluster, uint32_t entryCount, char *path) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    char name[9];
    char extension[4];

    int i = 0, dot = 0;
    for (; i < 8; i++) {
        if (path[i] == '.' || path[i] == '\0') break;
        name[i] = path[i];
    }
    if (path[i] == '.') dot = i;
    for (; i < 8; i++) {
        name[i] = ' ';
    }
    i = dot;
    name[8] = '\0';

    if (path[i] == '.') {
        int j = 0;
        i++; // Skip the dot
        for (; j < 3 && path[i] != '\0'; i++, j++) {
            extension[j] = path[i];
        }
        for (; j < 3; j++) {
            extension[j] = ' ';
        }
    } else {
        for (int j = 0; j < 3; j++) {
            extension[j] = ' ';
        }
    }
    

    if (name[0] == 0xE5)
        name[0] == 0x05;

    i = 0;
    for (; i < entryCount; i++) {
        if (!entryIsDirectory(&((*dir)[i]))) {
            if (strequal_nocase((*dir)[i].fileName, name, 8)) {
                vgaWriteln("File already exists");
                return false;
            }
        }
    }

    int dirIdx = -1;
    i = 0;
    for (; i < entryCount; i++) {
        if (entryIsAvailable(&((*dir)[i]))) {
            dirIdx = i;
            break;
        }
    }

    if (dirIdx < 0) {
        vgaWriteln("No free entries");
        return false;
    }
    uint16_t cluster = (uint16_t)findFreeCluster(fat, fatBytes / 2);
    fat[cluster] = 0xFFFF;
    memcpy(name, (*dir)[dirIdx].fileName, 8);
    memcpy(extension, (*dir)[dirIdx].fileExtension, 3);

    (*dir)[dirIdx].firstCluster = cluster;
    (*dir)[dirIdx].flags = FAT16_FLAG_ARCHIVE;
    (*dir)[dirIdx].size = 0;

    if (dirCluster == 0)
        diskWrite(fs->disk, rootDirStart, sectors, (byte*)(*dir));
    else
        writeChain(fs, fat, dirCluster, (byte*)(*dir), entryCount * sizeof(Fat16DirectoryEntry));
    
    return true;
}

static bool dirExists(Fat16FilesystemInfo *fs, uint16_t *fat, Fat16DirectoryEntry **dir, uint32_t dirCluster, uint32_t entryCount, char *path) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    char name[9];
    char extension[4];

    int i = 0;
    for (; i < 8; i++) {
        if (path[i] == '.' || path[i] == '\0') break;
        name[i] = path[i];
    }
    for (; i < 8; i++) {
        name[i] = ' ';
    }
    name[8] = '\0';
    
    if (name[0] == 0xE5)
        name[0] == 0x05;
        
    i = 0;
    for (; i < entryCount; i++) {
        if (entryIsDirectory(&((*dir)[i]))) {
            if (strequal_nocase((*dir)[i].fileName, name, 8)) {
                return true;
            }
        }
    }
    return false;
}

static bool fileExists(Fat16FilesystemInfo *fs, uint16_t *fat, Fat16DirectoryEntry **dir, uint32_t dirCluster, uint32_t entryCount, char *path) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    char name[9];
    char extension[4];

    int i = 0, dot = 0;
    for (; i < 8; i++) {
        if (path[i] == '.' || path[i] == '\0') break;
        name[i] = path[i];
    }
    if (path[i] == '.') dot = i;
    for (; i < 8; i++) {
        name[i] = ' ';
    }
    i = dot;
    name[8] = '\0';

    if (path[i] == '.') {
        int j = 0;
        i++; // Skip the dot
        for (; j < 3 && path[i] != '\0'; i++, j++) {
            extension[j] = path[i];
        }
        for (; j < 3; j++) {
            extension[j] = ' ';
        }
    } else {
        for (int j = 0; j < 3; j++) {
            extension[j] = ' ';
        }
    }
    
    if (name[0] == 0xE5)
        name[0] == 0x05;

    i = 0;
    for (; i < entryCount; i++) {
        if (entryIsFile(&((*dir)[i]))) {
            if (strequal_nocase((*dir)[i].fileName, name, 8) && strequal_nocase((*dir)[i].fileExtension, extension, 3)) {
                return true;
            }
        }
    }
    return false;
}

static bool readFile(Fat16FilesystemInfo *fs, uint16_t *fat, Fat16DirectoryEntry **dir, uint32_t dirCluster, uint32_t entryCount, char *path, byte *buffer, uint32_t nbytes) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    char name[9];
    char extension[4];

    int i = 0, dot = 0;
    for (; i < 8; i++) {
        if (path[i] == '.' || path[i] == '\0') break;
        name[i] = path[i];
    }
    if (path[i] == '.') dot = i;
    for (; i < 8; i++) {
        name[i] = ' ';
    }
    i = dot;
    name[8] = '\0';

    if (path[i] == '.') {
        int j = 0;
        i++; // Skip the dot
        for (; j < 3 && path[i] != '\0'; i++, j++) {
            extension[j] = path[i];
        }
        for (; j < 3; j++) {
            extension[j] = ' ';
        }
    } else {
        for (int j = 0; j < 3; j++) {
            extension[j] = ' ';
        }
    }
    
    if (name[0] == 0xE5)
        name[0] == 0x05;

    i = 0;
    int dirIdx = -1;
    for (; i < entryCount; i++) {
        if (entryIsFile(&((*dir)[i]))) {
            if (strequal_nocase((*dir)[i].fileName, name, 8) && strequal_nocase((*dir)[i].fileExtension, extension, 3)) {
                dirIdx = i;
                break;
            }
        }
    }
    if (dirIdx < 0) {
        vgaWriteln("File does not exist");
        return false;
    }

    uint16_t cluster = (uint16_t)(*dir)[dirIdx].firstCluster;
    uint32_t size = (*dir)[dirIdx].size;
    
    readChain(fs, fat, cluster, buffer, nbytes < size ? nbytes : size);
    return true;
}

static bool writeFile(Fat16FilesystemInfo *fs, uint16_t *fat, Fat16DirectoryEntry **dir, uint32_t dirCluster, uint32_t entryCount, char *path, byte *buffer, uint32_t nbytes) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    char name[9];
    char extension[4];

    int i = 0, dot = 0;
    for (; i < 8; i++) {
        if (path[i] == '.' || path[i] == '\0') break;
        name[i] = path[i];
    }
    if (path[i] == '.') dot = i;
    for (; i < 8; i++) {
        name[i] = ' ';
    }
    i = dot;
    name[8] = '\0';

    if (path[i] == '.') {
        int j = 0;
        i++; // Skip the dot
        for (; j < 3 && path[i] != '\0'; i++, j++) {
            extension[j] = path[i];
        }
        for (; j < 3; j++) {
            extension[j] = ' ';
        }
    } else {
        for (int j = 0; j < 3; j++) {
            extension[j] = ' ';
        }
    }
    
    if (name[0] == 0xE5)
        name[0] == 0x05;

    i = 0;
    int dirIdx = -1;
    for (; i < entryCount; i++) {
        if (entryIsFile(&((*dir)[i]))) {
            if (strequal_nocase((*dir)[i].fileName, name, 8) && strequal_nocase((*dir)[i].fileExtension, extension, 3)) {
                dirIdx = i;
                break;
            }
        }
    }
    if (dirIdx < 0) {
        vgaWriteln("File does not exist");
        return false;
    }
    vgaWriteStatic((*dir)[dirIdx].fileName, 8); vgaNextLine();
    uint16_t cluster = (uint16_t)(*dir)[dirIdx].firstCluster;
    fitChainToBytes(fs, fat, cluster, nbytes);
    writeChain(fs, fat, cluster, buffer, nbytes);

    (*dir)[dirIdx].size = nbytes;
    (*dir)[dirIdx].flags |= FAT16_FLAG_ARCHIVE;

    if (dirCluster == 0)
        diskWrite(fs->disk, rootDirStart, sectors, (byte*)(*dir));
    else
        writeChain(fs, fat, dirCluster, (byte*)(*dir), entryCount * sizeof(Fat16DirectoryEntry));
    return true;
}

static bool fat16CreateDirectorySingle(Fat16FilesystemInfo *fs, char *path) {
    if (path[0] == '/') path++;
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint16_t *fat = (uint16_t*)malloc(fatBytes);
    readFAT(fs, fat);
    // load root directory
    Fat16DirectoryEntry *dir = (Fat16DirectoryEntry*)malloc(bootsector->rootDirCount * sizeof(Fat16DirectoryEntry));
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    uint32_t entryCount = bootsector->rootDirCount;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    uint32_t dirCluster = 0;
    uint32_t parentDirCluster = 0;
    diskRead(fs->disk, rootDirStart, sectors, (byte*)dir);

    if (!traversePath(fs, fat, &dir, &parentDirCluster, &dirCluster, &entryCount, &path))
    {
        vgaWriteln("Path does not exist");
        free((void*)fat);
        free((void*)dir);
        return false;
    }

    if (!createDirectory(fs, fat, &dir, parentDirCluster, dirCluster, entryCount, path)) {
        free((void*)fat);
        free((void*)dir);
        return false;
    }

    writeFAT(fs, fat);

    free((void*)fat);
    free((void*)dir);
    return true;
}

bool fat16Setup(DiskInfo *diskInfo, Fat16FilesystemInfo *fs) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    fs->disk = diskInfo;
    readBootsector(fs);

    uint32_t firstFAT = bootsector->reservedSectors;
    byte *fatSector1 = (byte*) malloc(512);
    diskRead(diskInfo, firstFAT, 1, fatSector1);

    bool initialized = fatSector1[0] == bootsector->mediaDescriptorType;
    bool wasOk = true;
    if(initialized) {
        LOG("FAT16 is already ok\n");
    } else {
        LOG("FAT16 is not ok, setting up...\n");
        fatSector1[0] = bootsector->mediaDescriptorType;
        fatSector1[1] = 0xFF;
        fatSector1[2] = 0xFF;
        fatSector1[3] = 0xFF;
        diskWrite(diskInfo, firstFAT, 1, fatSector1);
        wasOk = false;
    }
    free((void*)fatSector1);
    fat16CreateDirectorySingle(fs, ".");
    fat16CreateDirectorySingle(fs, "..");
    return wasOk;
}

bool fat16CreateDirectory(Fat16FilesystemInfo *fs, char *path) {
    vgaWrite("Creating directory `");
    vgaWrite(path);
    vgaWrite("`... ");
    if (!fat16CreateDirectorySingle(fs, path)) return false;
    int len = strlen(path);
    char *pathLoopback = (char*)malloc(len+4);
    memcpy(path, pathLoopback, len);
    pathLoopback[len] = '/';
    pathLoopback[len+1] = '.';
    pathLoopback[len+2] = '\0';
    fat16CreateDirectorySingle(fs, pathLoopback);
    pathLoopback[len+2] = '.';
    pathLoopback[len+3] = '\0';
    fat16CreateDirectorySingle(fs, pathLoopback);
    free((void*)pathLoopback);
    vgaWriteln("OK");
    return true;
}

bool fat16CreateFile(Fat16FilesystemInfo *fs, char *path) {
    vgaWrite("Creating file `");
    vgaWrite(path);
    vgaWrite("`... ");
    if (path[0] == '/') path++;
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint16_t *fat = (uint16_t*)malloc(fatBytes);
    readFAT(fs, fat);

    // Load root directory
    Fat16DirectoryEntry *dir = (Fat16DirectoryEntry*)malloc(bootsector->rootDirCount * sizeof(Fat16DirectoryEntry));
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    uint32_t entryCount = bootsector->rootDirCount;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    uint32_t dirCluster = 0;
    uint32_t parentDirCluster = 0;
    diskRead(fs->disk, rootDirStart, sectors, (byte*)dir);

    if (!traversePath(fs, fat, &dir, &parentDirCluster, &dirCluster, &entryCount, &path)) {
        vgaWriteln("Path does not exist");
        free((void*)fat);
        free((void*)dir);
        return false;
    }

    if (!createFile(fs, fat, &dir, dirCluster, entryCount, path)) {
        free((void*)fat);
        free((void*)dir);
        return false;
    }

    writeFAT(fs, fat);

    free((void*)fat);
    free((void*)dir);
    vgaWriteln("OK");
    return true;
}

bool fat16WriteFile(Fat16FilesystemInfo *fs, char *path, byte *buffer, uint32_t nbytes) {
    vgaWrite("Writing to file `");
    vgaWrite(path);
    vgaWrite("`... ");
    if (path[0] == '/') path++;
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint16_t *fat = (uint16_t*)malloc(fatBytes);
    readFAT(fs, fat);

    // Load root directory
    Fat16DirectoryEntry *dir = (Fat16DirectoryEntry*)malloc(bootsector->rootDirCount * sizeof(Fat16DirectoryEntry));
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    uint32_t entryCount = bootsector->rootDirCount;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    uint32_t dirCluster = 0;
    uint32_t parentDirCluster = 0;
    diskRead(fs->disk, rootDirStart, sectors, (byte*)dir);

    if (!traversePath(fs, fat, &dir, &parentDirCluster, &dirCluster, &entryCount, &path)) {
        vgaWriteln("Path does not exist");
        free((void*)fat);
        free((void*)dir);
        return false;
    }

    if (!writeFile(fs, fat, &dir, dirCluster, entryCount, path, buffer, nbytes)) {
        free((void*)fat);
        free((void*)dir);
        return false;
    }

    writeFAT(fs, fat);

    free((void*)fat);
    free((void*)dir);
    vgaWriteln("OK");
    return true;
}

bool fat16ReadFile(Fat16FilesystemInfo *fs, char *path, byte *buffer, uint32_t nbytes) {
    vgaWrite("Reading file `");
    vgaWrite(path);
    vgaWrite("`... ");
    if (path[0] == '/') path++;
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint16_t *fat = (uint16_t*)malloc(fatBytes);
    readFAT(fs, fat);

    // Load root directory
    Fat16DirectoryEntry *dir = (Fat16DirectoryEntry*)malloc(bootsector->rootDirCount * sizeof(Fat16DirectoryEntry));
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    uint32_t entryCount = bootsector->rootDirCount;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    uint32_t dirCluster = 0;
    uint32_t parentDirCluster = 0;
    diskRead(fs->disk, rootDirStart, sectors, (byte*)dir);

    if (!traversePath(fs, fat, &dir, &parentDirCluster, &dirCluster, &entryCount, &path)) {
        vgaWriteln("Path does not exist");
        free((void*)fat);
        free((void*)dir);
        return false;
    }
    if (!readFile(fs, fat, &dir, dirCluster, entryCount, path, buffer, nbytes)) {
        free((void*)fat);
        free((void*)dir);
        return false;
    }
    
    writeFAT(fs, fat);
    

    free((void*)fat);
    free((void*)dir);
    vgaWriteln("OK");
    return true;
}

bool fat16PathExists(Fat16FilesystemInfo *fs, char *path) {
    //vgaWrite("Looking for existence file `");
    //vgaWrite(path);
    //vgaWrite("`... ");
    if (path[0] == '/') path++;
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint16_t *fat = (uint16_t*)malloc(fatBytes);
    readFAT(fs, fat);

    // Load root directory
    Fat16DirectoryEntry *dir = (Fat16DirectoryEntry*)malloc(bootsector->rootDirCount * sizeof(Fat16DirectoryEntry));
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    uint32_t entryCount = bootsector->rootDirCount;
    uint8_t entriesPerSector = bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry);
    uint8_t sectors = entryCount / entriesPerSector;
    uint32_t dirCluster = 0;
    uint32_t parentDirCluster = 0;
    diskRead(fs->disk, rootDirStart, sectors, (byte*)dir);

    if (!traversePath(fs, fat, &dir, &parentDirCluster, &dirCluster, &entryCount, &path)) {
        free((void*)fat);
        free((void*)dir);
        return false;
    }
    if (fileExists(fs, fat, &dir, dirCluster, entryCount, path)) {
        free((void*)fat);
        free((void*)dir);
        return true;
    }
    if (dirExists(fs, fat, &dir, dirCluster, entryCount, path)) {
        free((void*)fat);
        free((void*)dir);
        return true;
    }
    
    free((void*)fat);
    free((void*)dir);
    return false;
}

void printRootDirectory(Fat16FilesystemInfo *fs) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t entryCount = bootsector->rootDirCount;

    Fat16DirectoryEntry *dir = (Fat16DirectoryEntry *)malloc(entryCount * sizeof(Fat16DirectoryEntry));
    diskRead(fs->disk, rootDirStart, entryCount / (bootsector->bytesPerSector / sizeof(Fat16DirectoryEntry)), (byte *)dir);
    char filename[13];
    char filename83[12];
    for (uint32_t i = 0; i < entryCount; i++) {
        if (!entryIsAvailable(&dir[i])) {
            
            createFilename83(dir[i].fileName, dir[i].fileExtension, filename83);
            convertFilename83ToRegular(filename83, filename);
            if (entryIsDirectory(&dir[i]))
                append(filename, '/');
            vgaWriteln(filename);
        }
    }

    free((void *)dir);
}