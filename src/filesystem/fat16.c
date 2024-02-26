#include "fat16.h"

#include "../debug.h"
#include "../libc/mem.h"

void fat16ReadBootsector(Fat16FilesystemInfo *fs) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    diskRead(fs->disk, 0, 1, (byte*) bootsector);
}

void fat16Setup(DiskInfo *diskInfo, Fat16FilesystemInfo *fs) {
    Fat16BootSector *bootsector = &(fs->bootsector);
    fs->disk = diskInfo;
    fat16ReadBootsector(fs);

    uint32_t firstFAT = bootsector->reservedSectors;
    byte *fatSector1 = (byte*) malloc(512);
    diskRead(diskInfo, firstFAT, 1, fatSector1);

    LOG_BYTE(fatSector1[0]); LOG("\n");

    bool initialized = fatSector1[0] == bootsector->mediaDescriptorType;
    initialized &= fatSector1[1] == 0xFF;
    initialized &= fatSector1[2] == 0xFF;
    initialized &= fatSector1[3] == 0xFF;

    if(initialized) {
        LOG("FAT16 is already ok");
    } else {
        LOG("FAT16 is not ok, setting up...");
        memorySet(fatSector1, 0xFF, 512);

        for(int i = 0; i < bootsector->fatCount; i++)
            diskWrite(diskInfo, firstFAT+i, 1, fatSector1);
        
        fatSector1[0] = bootsector->mediaDescriptorType;
        diskWrite(diskInfo, firstFAT, 1, fatSector1);
    }
    dealloc((void*)fatSector1);
}