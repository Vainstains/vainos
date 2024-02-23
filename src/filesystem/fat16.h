#ifndef FAT16_H
#define FAT16_H

#include "../drivers/disk.h"

#include "../types.h"

#define FAT16_EntryBytes 32

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
    byte hour;
    byte minute;
    byte second;
} Fat16Time;

typedef struct {
    byte year;
    byte month;
    byte day;
} Fat16Date;

typedef struct {
    char fileName[11];
    byte flags;
    byte _ntReserved;
    byte creationTimeSecondsFrac;
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t accessedDate;
    uint16_t firstClusterHi;
    uint16_t modificationTime;
    uint16_t modificationDate;
    uint16_t firstClusterLo;
    uint32_t size;
}__attribute__((packed)) Fat16DirectoryEntry;

typedef struct {
    byte order;
    byte first5[10];
    byte attribute;
    byte longEntryType;
    byte shortNameChecksum;
    byte next6[12];
    word _zero;
    byte final2[4];
}__attribute__((packed)) Fat16LongFileNameEntry;

void fat16ReadBootsector(DiskInfo *disk, Fat16BootSector *bootsector);

#endif // FAT16_H