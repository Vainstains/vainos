#include "atapio.h"

#include "../drivers/ports.h"
#include "../debug.h"

// LBA value: the sector offset from the very beginning of the disk
//            completely ignoring partition boundaries

static void waitStatusRead() {
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);

    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);

    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
    portByteIn(ATAPIO_Port_CommStat);
}

void waitBSYClear() {
    while (portByteIn(ATAPIO_Port_CommStat) & 0x80) {}  // wait until BSY clears
}

bool atapioIdentify(uint8_t target, uint16_t *buffer) {
    LOG("Identifying drive "); LOG_BYTE(target); LOG("... ");

    portByteOut(ATAPIO_Port_DriveSelect, target);

    waitStatusRead();
    waitBSYClear();

    portByteOut(ATAPIO_Port_SectorCount, 0x00);
    portByteOut(ATAPIO_Port_LBAlo, 0x00);
    portByteOut(ATAPIO_Port_LBAmid, 0x00);
    portByteOut(ATAPIO_Port_LBAhi, 0x00);
    portByteOut(ATAPIO_Port_CommStat, 0xEC); // Identify command = 0xEC
    
    waitBSYClear();

    uint8_t status = portByteIn(ATAPIO_Port_CommStat);
    
    LOG("[");LOG_BYTE(status);LOG("] ");
    if (status == 0) {
        LOG("Not a drive, RIP (status was 0)\n");
        return false; // not a drive
    }
    waitBSYClear();

    uint8_t mid = portByteIn(ATAPIO_Port_LBAmid);
    uint8_t hi = portByteIn(ATAPIO_Port_LBAhi);

    if (mid || hi) {
        LOG("Not ATA compliant :( (LBAmid or LBAhi was not 0)\n");
        return false; // not ATA
    }

    // (DRQ_bit or ERR_bit) == (8 | 1) == 9
    while ((portByteIn(ATAPIO_Port_CommStat) & 9) == 0) {} // wait until DRQ sets or ERR sets
    
    if ((portByteIn(ATAPIO_Port_CommStat) & 1) == 1) {
        LOG("Disk error, F in chat (ERR bit was set)\n");
        return false; // ERR was set
    }

    for (size_t i = 0; i < 256; i++) {
        buffer[i] = portWordIn(ATAPIO_Port_Data);
    }
    LOG("All seems good :D\n");
    return true;
}

void atapioInit() {
    
}

void atapioRead28(uint8_t target, uint32_t LBA, uint8_t sectorCount, uint8_t *buffer) {
    portByteOut(ATAPIO_Port_DriveSelect, target | ((LBA >> 24) & 0x0F));

    waitStatusRead();
    waitBSYClear();

    portByteOut(ATAPIO_Port_Error, 0x00);
    portByteOut(ATAPIO_Port_SectorCount, sectorCount);
    portByteOut(ATAPIO_Port_LBAlo, (uint8_t)LBA);
    portByteOut(ATAPIO_Port_LBAmid, (uint8_t)(LBA >> 8));
    portByteOut(ATAPIO_Port_LBAhi, (uint8_t)(LBA >> 16));
    portByteOut(ATAPIO_Port_CommStat, 0x20); // Read Sectors command = 0x20

    waitBSYClear();

    size_t idx = 0;
    uint16_t *wordbuf = (uint16_t*)buffer;
    for (size_t i = 0; i < sectorCount; i++) {
        for (size_t j = 0; j < 256; j++) {
            wordbuf[idx++] = portWordIn(ATAPIO_Port_Data);
        }
        waitBSYClear();
    }
}

void atapioWrite28(uint8_t target, uint32_t LBA, uint8_t sectorCount, const uint8_t *buffer) {
    portByteOut(ATAPIO_Port_DriveSelect, target | ((LBA >> 24) & 0x0F));

    waitStatusRead();
    waitBSYClear();

    portByteOut(ATAPIO_Port_Error, 0x00);
    portByteOut(ATAPIO_Port_SectorCount, sectorCount);
    portByteOut(ATAPIO_Port_LBAlo, (uint8_t)LBA);
    portByteOut(ATAPIO_Port_LBAmid, (uint8_t)(LBA >> 8));
    portByteOut(ATAPIO_Port_LBAhi, (uint8_t)(LBA >> 16));
    portByteOut(ATAPIO_Port_CommStat, 0x30); // Write Sectors command = 0x30

    waitBSYClear();

    size_t idx = 0;
    uint16_t *wordbuf = (uint16_t*)buffer;
    for (size_t i = 0; i < sectorCount; i++) {
        for (size_t j = 0; j < 256; j++) {
            portWordOut(ATAPIO_Port_Data, wordbuf[idx++]);
        }
        waitBSYClear();
    }
    portByteOut(ATAPIO_Port_CommStat, 0xE7); // Cache Flush command = 0xE7
    waitBSYClear();
}