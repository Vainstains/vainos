#include "fat16.h"

#include "../debug.h"
#include "../libc/mem.h"
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
    uint32_t length = 0;
    uint16_t value;
    while (1) {
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

        fat[endCluster] = (uint16_t)newCluster;
        fat[newCluster] = (i == n - 1) ? 0xFFFF : (uint16_t)(newCluster + 1);

        clearCluster(fs, newCluster);
        endCluster = newCluster;
    }
    writeFAT(fs, fat);
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
    writeFAT(fs, fat);
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

typedef struct {
    uint32_t entryCount;
    uint32_t startCluster;
    Fat16DirectoryEntry *entries;
} Fat16Subdirectory;

static void readRootDirectory(Fat16FilesystemInfo *fs, Fat16Subdirectory *dir) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    dir->entryCount = bootsector->rootDirCount;
    uint8_t entriesPerSector = bootsector->bytesPerSector / 32;
    uint8_t sectors = dir->entryCount / entriesPerSector;
    dir->startCluster = 0;
    diskRead(fs->disk, rootDirStart, sectors, (byte*) dir->entries);
}

static void writeRootDirectory(Fat16FilesystemInfo *fs, Fat16Subdirectory *dir) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    uint8_t entriesPerSector = bootsector->bytesPerSector / 32;
    diskWrite(fs->disk, rootDirStart, dir->entryCount / entriesPerSector, (byte*) dir->entries);
}

static void writeDirectory(Fat16FilesystemInfo *fs, uint16_t *fat, Fat16Subdirectory *dir) {
    if (dir->startCluster == 0) {
        writeRootDirectory(fs, dir);
        return;
    }
    byte *data = (byte*)(dir->entries);
    uint32_t dataLength = dir->entryCount * 32;
    writeChain(fs, fat, dir->startCluster, data, dataLength);
}

static void *readDirectoryAlloc(Fat16FilesystemInfo *fs, uint16_t *fat, Fat16Subdirectory *dir, uint32_t cluster) {
    Fat16BootSector *bootsector = &(fs->bootsector);

    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;

    uint32_t entriesPerCluster = bootsector->sectorsPerCluster * bootsector->bytesPerSector / 32;

    uint32_t totalEntries = entriesPerCluster * chainLength(fat, cluster);
    dir->entries = (Fat16DirectoryEntry *)malloc(totalEntries * sizeof(Fat16DirectoryEntry));

    uint32_t currentCluster = cluster;
    uint32_t currentEntry = 0;

    while (currentCluster < 0xFFF8) {
        readCluster(fs, currentCluster, (byte *)(dir->entries) + currentEntry * sizeof(Fat16DirectoryEntry));
        currentCluster = fat[currentCluster];
        currentEntry += entriesPerCluster;
    }

    dir->entryCount = totalEntries;
    dir->startCluster = cluster;
    return (void*)(dir->entries);
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

static void convertFilenameRegularTo83(char *name, char *dst) {
    uint8_t dstIdx = 0;
    uint8_t i = 0;
    for (; i < 8; i++) {
        if (name[i] == ' ' || name[i] == '\0' || name[i] == '.')
            break;
        dst[dstIdx++] = name[i];
    }
    for (; dstIdx < 8; dstIdx++) {
        dst[dstIdx] = ' ';
    }

    while (name[i] != '\0' && name[i] != '.')
        i++;
    
    i++;
    for (dstIdx = 8; dstIdx < 11 && name[i] != '\0'; i++, dstIdx++) {
        dst[dstIdx] = name[i];
    }
    for (; dstIdx < 11; dstIdx++) {
        dst[dstIdx] = ' ';
    }
    if (dst[0] == 0xE5)
        dst[0] = 0x05;
}

static bool fileExistsInDir(Fat16Subdirectory *dir, char *filename) {
    char tmpName[12];
    
    for (uint32_t i = 0; i < dir->entryCount; i++) {
        if (!entryIsFile(&(dir->entries[i]))) {
            continue;
        }
        convertFilename83ToRegular(dir->entries[i].fileName, tmpName);
        if (strcmp(tmpName, filename) == 0) {
            return true;
        }
    }
    return false;
}

static bool subdirectoryExistsInDir(Fat16Subdirectory *dir, char *subdirName) {
    char tmpName[12];
    
    for (uint32_t i = 0; i < dir->entryCount; i++) {
        if (!entryIsDirectory(&(dir->entries[i]))) {
            continue;
        }
        convertFilename83ToRegular(dir->entries[i].fileName, tmpName);
        if (strcmp(tmpName, subdirName) == 0) {
            return true;
        }
    }
    return false;
}

static void createNewFile(Fat16FilesystemInfo *fs, char *name, uint16_t *fat, Fat16Subdirectory *parent) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    if (fileExistsInDir(parent, name)) {
        return;
    }

    Fat16DirectoryEntry *entry = parent->entries;
    for (uint32_t i = 0; i < parent->entryCount; i++) {
        if (entryIsAvailable(entry)) {
            break;
        }
        entry++;
    }

    convertFilenameRegularTo83(name, entry->fileName);
    entry->size = 0;
    entry->flags = 0;
    uint32_t freeCluster = findFreeCluster(fat, bootsector->sectorsPerFAT * bootsector->bytesPerSector / 2);
    fat[freeCluster] = 0xFFFF;
    clearCluster(fs, freeCluster);

    entry->firstClusterLo = (uint16_t)freeCluster;

    writeDirectory(fs, fat, parent);

    writeFAT(fs, fat);
}

static void createNewSubdirectory(Fat16FilesystemInfo *fs, char *subdirName, uint16_t *fat, Fat16Subdirectory *parent) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    if (subdirectoryExistsInDir(parent, subdirName)) {
        return;
    }

    Fat16DirectoryEntry *entry = parent->entries;
    for (uint32_t i = 0; i < parent->entryCount; i++) {
        if (entryIsAvailable(entry)) {
            break;
        }
        entry++;
    }

    convertFilenameRegularTo83(subdirName, entry->fileName);
    entry->size = 0;
    entry->flags = FAT16_FLAG_DIRECTORY;
    uint32_t freeCluster = findFreeCluster(fat, bootsector->sectorsPerFAT * bootsector->bytesPerSector / 2);
    fat[freeCluster] = 0xFFFF;
    clearCluster(fs, freeCluster);

    entry->firstClusterLo = (uint16_t)freeCluster;

    writeDirectory(fs, fat, parent);

    writeFAT(fs, fat);
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
    free((void*)fatSector1);
}

byte fat16CreateFile(Fat16FilesystemInfo *fs, char *name) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t fatBytes = bootsector->bytesPerSector * bootsector->sectorsPerFAT;
    uint16_t *fat = (uint16_t*)malloc(fatBytes);
    readFAT(fs, fat);

    Fat16Subdirectory dir;
    dir.entries = (Fat16DirectoryEntry*)malloc(32*bootsector->rootDirCount);
    readRootDirectory(fs, &dir);
    
    createNewFile(fs, name, fat, &dir);

    free((void*)fat);
    free((void*)dir.entries);
    return 1;
}

void fat16PrintRootDir(Fat16FilesystemInfo *fs) {
    Fat16BootSector *bootsector = &(fs->bootsector);

    Fat16Subdirectory rootdir;
    rootdir.entries = (Fat16DirectoryEntry*)malloc(32*bootsector->rootDirCount);
    readRootDirectory(fs, &rootdir);
    char tmpName[12];
    vgaWriteln("Files in root directory:");
    Fat16DirectoryEntry *entry = rootdir.entries;
    for (uint16_t i = 0; i < rootdir.entryCount; i++) {
        if (!entryIsAvailable(entry)) {
            convertFilename83ToRegular(entry->fileName, tmpName);
            vgaWrite("  ");
            vgaWrite(tmpName);
            vgaNextLine();
        }
        entry++;
    }

    free((void*)rootdir.entries);
}