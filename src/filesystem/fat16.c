#include "fat16.h"

#include "../debug.h"
#include "../libc/mem.h"

// Note to self: pretty sure this is how Fat16 works at a high level:

// -The FAT is for storing info about blocks of data.
// -The Each fat entry is just an int16, that being the index to the next cluster.
//   (An index of 0xFFFF marks the end of a chain, 0x0000 marks an unused cluster)
// -The root directory is what organizes the FAT into files and directories
// -The root directory is not special, as in subdirectories are just files with their own "inner root directories"
//   (this means it is recursive, if handled correctly there can be little to no distinction between the root dir and a regular dir)
// -Entries point to the first FAT index, just follow the chain

static bool readRootDirectory(Fat16FilesystemInfo *fs, Fat16DirectoryEntry *entries) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    diskRead(fs->disk, rootDirStart, bootsector->rootDirCount, (byte*) entries);

    return true;
}

static void readCluster(Fat16FilesystemInfo *fs, uint32_t cluster, byte *buffer) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;
    diskRead(fs->disk, clusterStart, bootsector->sectorsPerCluster, buffer);
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

static void readChain(Fat16FilesystemInfo *fs, uint16_t *fat, uint32_t cluster, byte *buffer) {
    Fat16BootSector *bootsector = &(fs->bootsector);

    uint32_t dataStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);
    uint32_t bufferOffset = 0;
    while (cluster < 0xFFF8) {
        uint32_t clusterStart = dataStart + (cluster - 2) * bootsector->sectorsPerCluster;
        readCluster(fs, cluster, buffer + bufferOffset);

        bufferOffset += bootsector->sectorsPerCluster * bootsector->bytesPerSector;
        cluster = fat[cluster];
    }
}

static uint32_t findFreeCluster(uint16_t *fat, uint32_t totalClusters) {
    for (uint32_t cluster = 2; cluster < totalClusters; cluster++) {
        if (fat[cluster] == 0x0000) {
            return cluster;
        }
    }
    return 0xFFFFFFFF;
}

static void readBootsector(Fat16FilesystemInfo *fs) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    diskRead(fs->disk, 0, 1, (byte*) bootsector);
}

static uint32_t directoryItemCount(Fat16DirectoryEntry *raw, uint32_t total) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < total; i++) {
        if (raw[i].fileName[0] != 0xE5 && raw[i].fileName[0] != 0x00) {
            count++;
        }
    }

    return count;
}

static bool validFilename(char byte0) { return !(byte0 == 0x00 || byte0 == 0xE5); }

static void extendChain(Fat16FilesystemInfo *fs, uint16_t *fat, uint32_t cluster, uint32_t targetLength) {

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