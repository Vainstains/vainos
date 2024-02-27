#ifndef DISK_H
#define DISK_H

#include "../types.h"

#define DISK_BACKEND_ATAPIO 0

typedef struct {
    bool allOK;
    byte backend;
    uint32_t sectors;   // Maximum disk capacity in sectors

    // backend-specific fields, used internally
    byte _atapio_id;
    byte _atapio_rw28id;
} DiskInfo;

bool diskGetATAPIO(byte id, DiskInfo *diskInfo);

bool diskRead(DiskInfo *diskInfo, uint32_t sector, uint8_t count, byte *buffer);

bool diskWrite(DiskInfo *diskInfo, uint32_t sector, uint8_t count, const byte *buffer);

#endif // DISK_H