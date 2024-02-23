#include "disk.h"

#include "atapio.h"

void getDiskATAPIO(byte id, DiskInfo *diskInfo) {
    diskInfo->backend = DISK_BACKEND_ATAPIO;
    diskInfo->_atapio_id = id;
    diskInfo->_atapio_rwid = id ? ATAPIO_Identify_Secondary : ATAPIO_Identify_Primary;

    uint16_t data[256];
    diskInfo->allOK = atapioIdentify(diskInfo->_atapio_rwid, data);

    uint32_t *data2 = (uint32_t*) data;
    diskInfo->sectors = data2[30];
}

void diskRead(DiskInfo *diskInfo, uint32_t sector, uint8_t count, byte *buffer) {
    if (!(diskInfo->allOK)) return;
    switch (diskInfo->backend)
    {
        case DISK_BACKEND_ATAPIO:
            atapioRead28(diskInfo->_atapio_rwid, sector, count, buffer);
            break;
    }
}

void diskWrite(DiskInfo *diskInfo, uint32_t sector, uint8_t count, const byte *buffer) {
    if (!(diskInfo->allOK)) return;
    switch (diskInfo->backend)
    {
        case DISK_BACKEND_ATAPIO:
            atapioWrite28(diskInfo->_atapio_rwid, sector, count, buffer);
            break;
    }
}