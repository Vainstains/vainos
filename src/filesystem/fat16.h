#ifndef FAT16_H
#define FAT16_H

#include "../drivers/disk.h"

#include "../types.h"

#define FAT16_EntryBytes 32

#define FAT16_FLAG_READONLY 0x01
#define FAT16_FLAG_HIDDEN 0x02
#define FAT16_FLAG_SYSTEM 0x04
#define FAT16_FLAG_VOLUMELABEL 0x08
#define FAT16_FLAG_DIRECTORY 0x10
#define FAT16_FLAG_ARCHIVE 0x20

typedef struct {
    // BIOS Parameter Block
    char jmp[3];
    char oem[8];
    uint16_t bytesPerSector;
    byte sectorsPerCluster;
    uint16_t reservedSectors;
    byte fatCount;
    uint16_t rootDirCount;
    uint16_t totalSectors;
    byte mediaDescriptorType;
    uint16_t sectorsPerFAT;
    uint16_t sectorsPerTrack;
    uint16_t headCount;
    uint32_t hiddenSectors;
    uint32_t largeSectorCount;

    // Extended Boot Record
    byte driveNumber;
    byte ntFlags;
    byte signature;
    uint32_t volumeID;
    char volumeLabel[11];
    char systemIdentifier[8];
    byte bootCode[448];
    uint16_t bootablePartitionSignature;
}__attribute__((packed)) Fat16BootSector;

typedef struct {
    char fileName[8];
    char fileExtension[3];
    byte flags;
    byte _ntReserved[10];
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t firstCluster;
    uint32_t size;
}__attribute__((packed)) Fat16DirectoryEntry;

typedef struct {
    DiskInfo *disk;
    Fat16BootSector bootsector;
} Fat16FilesystemInfo;

void fat16Setup(DiskInfo *disk, Fat16FilesystemInfo *fs);
bool fat16CreateDirectory(Fat16FilesystemInfo *fs, char *path);
bool fat16CreateFile(Fat16FilesystemInfo *fs, char *path);
bool fat16WriteFile(Fat16FilesystemInfo *fs, char *path, byte *buffer, uint32_t nbytes);
bool fat16ReadFile(Fat16FilesystemInfo *fs, char *path, byte *buffer, uint32_t nbytes);
void printRootDirectory(Fat16FilesystemInfo *fs);

#endif // FAT16_H