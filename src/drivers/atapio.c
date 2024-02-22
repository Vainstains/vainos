#include "atapio.h"

#include "../drivers/ports.h"
#include "../debug.h"

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

bool atapioIdentify(uint8_t target, uint16_t *data) {
    LOG("Identifying drive "); LOG_BYTE(target); LOG("...\n");
    portByteOut(ATAPIO_Port_CommStat, 0xEF);

    waitStatusRead();

    portByteOut(ATAPIO_Port_DriveSelect, 0xA0 | (target << 4));

    waitStatusRead();
    waitBSYClear();

    portByteOut(ATAPIO_Port_SectorCount, 0);
    portByteOut(ATAPIO_Port_LBAlo, 0);
    portByteOut(ATAPIO_Port_LBAmid, 0);
    portByteOut(ATAPIO_Port_LBAhi, 0);

    portByteIn(ATAPIO_Port_CommStat);
    portByteOut(ATAPIO_Port_CommStat, 0xEC);
    
    waitBSYClear();

    uint8_t status = portByteIn(ATAPIO_Port_CommStat);
    uint8_t mid = portByteIn(ATAPIO_Port_LBAmid);
    uint8_t hi = portByteIn(ATAPIO_Port_LBAhi);
    LOG("data:\n");
    LOG_BYTE(status);
    LOG(" (status)\n");
    LOG_BYTE(mid);
    LOG(" (mid)\n");
    LOG_BYTE(hi);
    LOG(" (hi)\n");

    if (status == 0) {
        LOG("Not a drive (status was 0)\n");
        return false; // not a drive
    }
    if (mid || hi) {
        LOG("Not ATA (LBAmid or LBAhi was not 0)\n");
        return false; // not ATA
    }

    // (DRQ_bit or ERR_bit) == (8 | 1) == 9
    while ((portByteIn(ATAPIO_Port_CommStat) & 9) == 0) {} // wait until DRQ sets or ERR sets
    
    if ((portByteIn(ATAPIO_Port_CommStat) & 1) == 1) {
        LOG(", Error (ERR bit was set)\n");
        return false; // ERR was set
    }

    return true;
}