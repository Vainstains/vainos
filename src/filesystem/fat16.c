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
    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;
    diskRead(fs->disk, clusterStart, bootsector->sectorsPerCluster, buffer);
}

static void writeCluster(Fat16FilesystemInfo *fs, uint32_t cluster, byte *buffer) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;
    diskWrite(fs->disk, clusterStart, bootsector->sectorsPerCluster, buffer);
}

static void clearCluster(Fat16FilesystemInfo *fs, uint32_t cluster) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
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

    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t bufferOffset = 0;

    while (cluster < 0xFFF8 && nbytes > 0) {
        uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;
        uint32_t bytesToRead = nbytes < (bootsector->sectorsPerCluster * bootsector->bytesPerSector) ?
                               nbytes : (bootsector->sectorsPerCluster * bootsector->bytesPerSector);

        readCluster(fs, cluster, buffer + bufferOffset);

        bufferOffset += bytesToRead;
        nbytes -= bytesToRead;

        cluster = fat[cluster];
    }
}

static void writeChain(Fat16FilesystemInfo *fs, uint16_t *fat, uint32_t cluster, byte *buffer, uint32_t nbytes) {
    Fat16BootSector *bootsector = &(fs->bootsector);

    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t bufferOffset = 0;

    while (cluster < 0xFFF8 && nbytes > 0) {
        uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;
        uint32_t bytesToWrite = nbytes < (bootsector->sectorsPerCluster * bootsector->bytesPerSector) ?
                                nbytes : (bootsector->sectorsPerCluster * bootsector->bytesPerSector);                   
        diskWrite(fs->disk, clusterStart, bootsector->sectorsPerCluster, buffer + bufferOffset);

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
    if (dst[0] == 0x05)
        dst[0] = 0xE5;
    dst[dstIdx] = 0; // null-term
}

void fat16Setup(DiskInfo *diskInfo, Fat16FilesystemInfo *fs) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    fs->disk = diskInfo;
    readBootsector(fs);

    uint32_t firstFAT = bootsector->reservedSectors;
    byte *fatSector1 = (byte*) malloc(512);
    diskRead(diskInfo, firstFAT, 1, fatSector1);

    LOG_BYTE(fatSector1[0]); LOG("\n");

    bool initialized = fatSector1[0] == bootsector->mediaDescriptorType;
    initialized &= fatSector1[1] == 0xFF;
    initialized &= fatSector1[2] == 0xFF;
    initialized &= fatSector1[3] == 0xFF;

    if(initialized) {
        LOG("FAT16 is already ok\n");
    } else {
        LOG("FAT16 is not ok, setting up...\n");
        fatSector1[0] = bootsector->mediaDescriptorType;
        fatSector1[1] = 0xFF;
        fatSector1[2] = 0xFF;
        fatSector1[3] = 0xFF;
        diskWrite(diskInfo, firstFAT, 1, fatSector1);
    }
    vgaWrite("FAT entries: "); vgaWriteInt((bootsector->bytesPerSector * bootsector->sectorsPerFAT)/2); vgaNextLine();
    free((void*)fatSector1);
}

void fat16CreateFolder(Fat16FilesystemInfo *fs, char *path) {
    vgaWrite("Creating folder: ");
    vgaWriteln(path);
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
    diskRead(fs->disk, rootDirStart, sectors, (byte*)dir);
    vgaWriteStatic((byte*)dir, 64); vgaNextLine();
    char name[9];
    int i, dirIdx;
    while (strcontains(path, '/')) {
        i = 0;
        for (; i < 8; i++) {
            if (path[i] == '/') break;
            name[i] = path[i];
        }
        for (; i < 8; i++) {
            name[i] = ' ';
        }
        name[8] = '\0';
        path = strchr(path, '/') + 1;
        vgaWrite("Looking for folder: ");
        vgaWrite(name);
        if (name[0] == 0xE5)
            name[0] == 0x05;
        i = 0;
        dirIdx = -1;
        for (; i < entryCount; i++) {
            if (entryIsDirectory(&(dir[i])))
            {
                //vgaWriteStatic(dir[i].fileName, 8); vgaNextLine();
                if (memequal(dir[i].fileName, name, 8)) {
                    dirIdx = i;
                    break;
                }
            }
        }
        if (dirIdx < 0)
        {
            vgaWriteln(" Not found!");
            free((void*)fat);
            free((void*)dir);
            return;
        }
        vgaWriteln("Found!");
        uint16_t newDirCluster = dir[i].firstCluster;
        uint32_t bytes = chainLength(fat, newDirCluster) * (uint32_t)bootsector->bytesPerSector * (uint32_t)bootsector->sectorsPerCluster;
        free((void*)dir);
        dir = (Fat16DirectoryEntry*)malloc(bytes);
        
        readChain(fs, fat, newDirCluster, (byte*)dir, bytes);
        vgaWriteStatic((byte*)dir, 64); vgaNextLine();
        dirCluster = newDirCluster;
        entryCount = bytes / sizeof(Fat16DirectoryEntry);
    }
    i = 0;
    dirIdx = -1;
    for (; i < entryCount; i++) {
        if (entryIsAvailable(&(dir[i])))
        {
            dirIdx = i;
            break;
        }
    }
    if (dirIdx < 0)
    {
        vgaWriteln("No free entries!");
        free((void*)fat);
        free((void*)dir);
        return;
    }
    i = 0;
    for (; i < 8; i++) {
        if (path[i] == '/' || path[i] == '\0') break;
        name[i] = path[i];
    }
    for (; i < 8; i++) {
        name[i] = ' ';
    }
    name[8] = '\0';
    path = strchr(path, '/') + 1;
    vgaWrite("Creating directory: ");
    vgaWriteln(name);
    if (name[0] == 0xE5)
        name[0] == 0x05;
    uint16_t cluster = (uint16_t)findFreeCluster(fat, fatBytes / 2);
    fat[cluster] = 0xFFFF;
    memcpy(name, dir[dirIdx].fileName, 8);
    dir[dirIdx].firstCluster = cluster;
    dir[dirIdx].flags = FAT16_FLAG_DIRECTORY;
    dir[dirIdx].size = (uint32_t)bootsector->bytesPerSector * (uint32_t)bootsector->sectorsPerCluster;
    if (dirCluster == 0)
        diskWrite(fs->disk, rootDirStart, sectors, (byte*)dir);
    else
        writeChain(fs, fat, dirCluster, (byte*)dir, dir[dirIdx].size);
    vgaWriteInt(dirCluster); vgaNextLine();
    //vgaWriteInt(dir[dirIdx].size); vgaNextLine();
    writeFAT(fs, fat);

    free((void*)fat);
    free((void*)dir);
}