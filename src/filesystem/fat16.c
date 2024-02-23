#include "fat16.h"
#include "../drivers/disk.h"

#include "../debug.h"
typedef struct {
    // BIOS Parameter Block
    char jmp[3];
    char oem[8];
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t fatCount;
    uint16_t rootDirCount;
    uint16_t totalSectors;
    uint8_t mediaDescriptorType;
    uint16_t sectorsPerFAT;
    uint16_t sectorsPerTrack;
    uint16_t headCount;
    uint32_t hiddenSectors;
    uint32_t largeSectorCount;

    // Extended Boot Record
    uint8_t driveNumber;
    uint8_t ntFlags;
    uint8_t signature;
    uint32_t volumeID;
    char volumeLabel[11];
    char systemIdentifier[8];
    uint8_t bootCode[448];
    uint16_t bootablePartitionSignature;
}__attribute__((packed)) Fat16BootSector;