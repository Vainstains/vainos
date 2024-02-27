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

typedef struct {
    uint32_t entryCount;
    Fat16DirectoryEntry *entries;
} Directory;

static void readRootDirectory(Fat16FilesystemInfo *fs, Directory *dir) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    dir->entryCount = bootsector->rootDirCount;
    uint8_t entriesPerSector = bootsector->bytesPerSector / 32;
    uint8_t sectors = dir->entryCount / entriesPerSector;
    diskRead(fs->disk, rootDirStart, sectors, (byte*) dir->entries);
}

static void writeRootDirectory(Fat16FilesystemInfo *fs, Directory *dir) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    uint8_t entriesPerSector = bootsector->bytesPerSector / 32;
    diskWrite(fs->disk, rootDirStart, dir->entryCount / entriesPerSector, (byte*) dir->entries);
}

static void readEntryDirectory(Fat16FilesystemInfo *fs, Fat16DirectoryEntry *entry, Directory *dir) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    uint32_t rootDirStart = bootsector->reservedSectors + (bootsector->fatCount * bootsector->sectorsPerFAT);

    dir->entryCount = bootsector->rootDirCount;
    uint8_t entriesPerSector = bootsector->bytesPerSector / 32;
    diskRead(fs->disk, rootDirStart, dir->entryCount / entriesPerSector, (byte*) dir->entries);
}

static uint32_t findFreeCluster(uint16_t *fat, uint32_t totalClusters) {
    for (uint32_t cluster = 2; cluster < totalClusters; cluster++) {
        if (fat[cluster] == 0x0000) {
            return cluster;
        }
    }
    return 0xFFFFFFFF;
}

static void readFAT(Fat16FilesystemInfo *fs, uint16_t *fat) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    diskRead(fs->disk, bootsector->reservedSectors, bootsector->sectorsPerFAT, (byte*) fat);
}

static void writeFAT(Fat16FilesystemInfo *fs, uint16_t *fat) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    diskWrite(fs->disk, bootsector->reservedSectors, bootsector->sectorsPerFAT, (byte*) fat);
}

static void readBootsector(Fat16FilesystemInfo *fs) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    diskRead(fs->disk, 0, 1, (byte*) bootsector);
}

static bool entryAvailable(char byte0) { return (byte0 == 0x00 || byte0 == 0xE5); }

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

void fat16CreateFile(Fat16FilesystemInfo *fs, char *name, char *ext) {
    // temporary funtion
    Fat16BootSector *bootsector = &(fs->bootsector);

    // 1.) read FAT and RD from disk
    uint16_t *fat = (uint16_t*)malloc(bootsector->bytesPerSector * bootsector->sectorsPerFAT);
    readFAT(fs, fat);
    Directory rootdir;
    
    LOG("a: ");
    LOG_INT(rootdir.entries); LOG("\n");
    rootdir.entries = (Fat16DirectoryEntry*)malloc(32*bootsector->rootDirCount);
    LOG("b: ");
    LOG_INT(rootdir.entries); LOG("\n");
    readRootDirectory(fs, &rootdir);
    LOG("RootDir:\n");
    LOG_INT(rootdir.entryCount); LOG("\n");
    
    // 2.) find available entry in RD

    Fat16DirectoryEntry *entry = rootdir.entries;
    for (uint32_t i = 0; i < rootdir.entryCount; i++) {
        if (entryAvailable(entry->fileName[0])) {
            LOG("Free entry found: idx "); LOG_INT(i); LOG("\n");
            break;
        }
    }
    
    // 3.) set entry name and general info
    createFilename83(name, ext, entry->fileName);
    vgaWriteStatic(entry->fileName, 11); vgaNextLine();

    // 4.) find available cluster in FAT and mark it 0xFFFF
    uint32_t freeCluster = findFreeCluster(fat, );
    // 5.) write zeroes to the found cluster on disk
    // 5.) set first cluster in RD entry to found cluster
    // 6.) write FAT and RD to disk
}