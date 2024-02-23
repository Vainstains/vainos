#include "fat16.h"

#include "../debug.h"

void fat16ReadBootsector(DiskInfo *diskInfo, Fat16BootSector *bootsector) {
    diskRead(diskInfo, 0, 1, (byte*) bootsector);
}

