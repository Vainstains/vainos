#include "disk.h"

#include "atapio.h"

#include "../debug.h"

void diskGetATAPIO(byte id, DiskInfo *diskInfo) {
    diskInfo->backend = DISK_BACKEND_ATAPIO;
    diskInfo->_atapio_id = id;
    diskInfo->_atapio_rw28id = id ? ATAPIO_ReadWrite28_Secondary : ATAPIO_ReadWrite28_Primary;

    uint16_t data[256];
    diskInfo->allOK = atapioIdentify(diskInfo->_atapio_rw28id, data);

    uint32_t *data2 = (uint32_t*) data;
    diskInfo->sectors = data2[30];
}

void diskRead(DiskInfo *diskInfo, uint32_t sector, uint8_t count, byte *buffer) {
    if (!(diskInfo->allOK)) {
        LOG("Cannot read from disk!\n");
        return;
    }
    switch (diskInfo->backend)
    {
        case DISK_BACKEND_ATAPIO:
            LOG("Reading with ATAPIO backend\n");
            atapioRead28(diskInfo->_atapio_rw28id, sector, count, buffer);
            break;
    }
}

void diskWrite(DiskInfo *diskInfo, uint32_t sector, uint8_t count, const byte *buffer) {
    if (!(diskInfo->allOK)) {
        LOG("Cannot write to disk!\n");
        return;
    }
    switch (diskInfo->backend)
    {
        case DISK_BACKEND_ATAPIO:
            LOG("Writing with ATAPIO backend\n");
            atapioWrite28(diskInfo->_atapio_rw28id, sector, count, buffer);
            break;
    }
}